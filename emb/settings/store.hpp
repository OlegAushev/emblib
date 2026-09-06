#pragma once

#include <emb/nvm/storage.hpp>
#include <emb/settings/image.hpp>
#include <emb/settings/record.hpp>

#include <array>
#include <bitset>
#include <expected>
#include <optional>
#include <span>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace settings {

// Where a section lives on a medium, and how much room it was given.
//
// Slots hold successive records; a save writes the next one and leaves the
// previous one intact, which is what makes an interrupted save harmless.
// Where erasing is required, slots are grouped into blocks the size of the
// medium's erase unit, and only the first slot of a block pays for an
// erase — by which time the newest record lives in another block.
//
//   FRAM:  {.magic = ..., .base = 0, .slot_capacity = 1024, .slot_count = 2}
//   flash: {..., .slot_capacity = 1024, .slot_count = 32,
//           .slots_per_block = 16}   // two 16 KB sectors
struct section {
  std::uint32_t magic;
  std::size_t base;
  std::size_t slot_capacity;
  std::size_t slot_count;
  std::size_t slots_per_block = 1;
};

// Which step of a save failed, and what the medium said about it. A cause
// is absent when the medium reported success but the record did not read
// back — the signature of a memory that is no longer holding data.
enum class save_stage : std::uint8_t {
  erase,
  body,
  commit,
  verify,
};

template<typename Error>
struct save_failure {
  save_stage stage;
  std::optional<Error> cause;
};

// What a load found, on top of what the record itself said.
struct load_result {
  load_report record;
  std::optional<std::size_t> slot;
  bool read_failed = false;
};

// Binds a schema to a place on a medium: finds the newest record that is
// whole, writes the next one, and never lets a failure destroy the last
// good copy.
template<auto& Schema, nvm::some_block_storage Storage, section Section>
class store {
  using addr_type = typename Storage::addr_type;
  using error_type = typename Storage::error_type;

  static constexpr std::size_t count = schema_t<Schema>::count;
  static constexpr std::size_t record_bytes = record_size(count);
  static constexpr std::size_t body_bytes = record_body_size(count);
  static constexpr std::size_t block_bytes = Section.slots_per_block
                                           * Section.slot_capacity;
  static constexpr std::size_t block_count = Section.slot_count
                                           / Section.slots_per_block;

  static_assert(Section.slot_count >= 2,
                "a store needs a second slot: a save must never be the only "
                "copy of the settings");
  static_assert(Section.slots_per_block >= 1);
  static_assert(Section.slot_count % Section.slots_per_block == 0);
  static_assert(!Storage::needs_erase || block_count >= 2,
                "erasing a block must never destroy the last good record, so "
                "the slots must span at least two erase blocks");
  static_assert(record_bytes <= Section.slot_capacity,
                "the record does not fit a slot");
  static_assert(Section.base + (Section.slot_count * Section.slot_capacity)
                    <= Storage::capacity,
                "the section does not fit the medium");
  static_assert(Section.base % Storage::write_granularity == 0);
  static_assert(Section.slot_capacity % Storage::write_granularity == 0);
  static_assert(body_bytes % Storage::write_granularity == 0
                    && record_footer_size % Storage::write_granularity == 0,
                "the medium cannot write the body and the footer separately, "
                "which is what commits a record");

  Storage& storage_;

  // A slot, not a record: a firmware that declared more parameters may have
  // written a longer record, and refusing to read it would silently discard
  // the settings of anyone downgrading.
  std::array<std::byte, Section.slot_capacity> buffer_{};

  std::size_t next_slot_ = 0;
  std::uint32_t last_seq_ = 0;
  bool surveyed_ = false;

  // Whether the slot the next save will take is known to be erased. It is
  // while a store runs — it advances one slot at a time and erases each
  // block on entering it — and it is not after the chain breaks: a restart
  // resumes from what the medium says, and a save that failed left the
  // position past whatever it managed to write.
  bool slot_ahead_unknown_ = false;

public:
  constexpr explicit store(Storage& storage) : storage_(storage) {}

  // Restores the image from the newest record that is whole. Tries the next
  // newest if one fails its checks, and falls back to defaults if none is
  // usable, so the image is defined whatever the medium holds.
  constexpr auto load(image<Schema>& values) -> load_result
  {
    load_result result;

    // Which slots have been looked at. Sized by the section rather than by
    // a machine word, so how many slots a section may have is the medium's
    // business and not this loop's.
    slot_set tried;

    // Where the medium stopped and what it holds are two questions. The
    // first candidate answers the first: it is the newest header there is,
    // whether or not the record behind it turns out to be whole. The next
    // record is numbered above it, so the debris of a save that never
    // committed never shares a generation with a record. The position,
    // though, follows the record restored and not the header: on two slots
    // the slot past the newest header is the record restored itself, and on
    // flash a lap-old header whose sequence number rotted upwards would put
    // the position in its block, from where a step over the debris lands in
    // the block of the record restored — and erases it.
    std::optional<candidate> newest;

    while (true) {
      auto const best = newest_untried(tried, result.read_failed);
      if (!best) break;
      tried.set(best->slot);
      if (!newest) newest = best;

      auto const stored = read_record(best->slot, result.read_failed);
      if (!stored) continue;

      auto const report = decode_record(*stored, Section.magic, values);
      if (!report.valid) continue;

      result.record = report;
      result.slot = best->slot;
      adopt(best->slot, newest->seq);
      return result;
    }

    values.restore_defaults();
    next_slot_ = 0;
    last_seq_ = 0;
    surveyed_ = true;
    slot_ahead_unknown_ = true;
    return result;
  }

  // Writes the image as the next record: body first, footer last, then
  // reads it back. Both the slot and the sequence number advance before the
  // first write, so a retry never lands on the debris of the attempt before
  // it, and never claims a generation that another record already claims.
  constexpr auto save(image<Schema> const& values)
      -> std::expected<void, save_failure<error_type>>
  {
    // A save before the first load would otherwise start counting from one
    // and write a record that looks older than what is already stored —
    // invisible to the next load, which takes the highest sequence number.
    if (!surveyed_) survey();
    if constexpr (Storage::needs_erase) {
      if (slot_ahead_unknown_) step_over_debris();
    }

    auto const slot = next_slot_;
    auto const seq = last_seq_ + 1;
    auto const record = std::span{buffer_}.first(record_bytes);

    encode_record(record, values, Section.magic, seq);

    // Spent whether or not the attempt succeeds. A failed save can still
    // have landed — the record wrote and only the read-back failed — and
    // reusing the number would leave two records claiming one generation,
    // where a load picks by slot order rather than by age. The one
    // exception is an erase that was refused, below.
    next_slot_ = (slot + 1) % Section.slot_count;
    last_seq_ = seq;

    if constexpr (Storage::needs_erase) {
      if (slot % Section.slots_per_block == 0) {
        auto const erased = storage_.erase(address_of(slot), block_bytes);
        if (!erased) {
          // Nothing was written, so there is no debris to move past, and
          // the block still has to be erased: the slot is not spent. Moving
          // on would put the position inside a block that was never
          // cleared, and the step over the debris would then take the next
          // block — the one holding the newest record.
          next_slot_ = slot;
          return fail(save_stage::erase, erased.error());
        }
      }
    }

    auto const body = storage_.write(address_of(slot),
                                     record.first(body_bytes));
    if (!body) return fail(save_stage::body, body.error());

    auto const footer = storage_.write(
        address_of(slot, body_bytes),
        record.subspan(body_bytes, record_footer_size));
    if (!footer) return fail(save_stage::commit, footer.error());

    if (auto const back = storage_.read(address_of(slot), record); !back) {
      return fail(save_stage::verify, back.error());
    }

    auto const header = decode_header(record, Section.magic);
    if (!header
        || header->seq != seq
        || detail::get_u32(record, record_bytes - 4)
               != detail::crc32(record.first(record_bytes - 4))) {
      return fail(save_stage::verify);
    }

    return {};
  }

  // Brings the whole section to the erased state — what an explicit "forget
  // the settings" command means. Honest on a medium with no erased state
  // too: erase() there overwrites.
  constexpr auto wipe() -> std::expected<void, error_type>
  {
    for (auto block = 0uz; block < block_count; ++block) {
      auto const at = address_of(block * Section.slots_per_block);
      if (auto const erased = storage_.erase(at, block_bytes); !erased) {
        return erased;
      }
    }
    next_slot_ = 0;
    last_seq_ = 0;
    surveyed_ = true;
    return {};
  }

  constexpr auto sequence() const -> std::uint32_t
  {
    return last_seq_;
  }

  constexpr auto next_slot() const -> std::size_t
  {
    return next_slot_;
  }

private:
  static constexpr auto address_of(std::size_t slot, std::size_t offset = 0)
      -> addr_type
  {
    return static_cast<addr_type>(
        Section.base + (slot * Section.slot_capacity) + offset);
  }

  // Every way out of a save that is not success. It leaves the slot ahead
  // in whatever state the failure left it — an erase that did not happen,
  // a record half written — so the next save has to look before it writes.
  constexpr auto fail(save_stage stage,
                      std::optional<error_type> cause = std::nullopt)
      -> std::unexpected<save_failure<error_type>>
  {
    slot_ahead_unknown_ = true;
    return std::unexpected(save_failure<error_type>{stage, cause});
  }

  struct candidate {
    std::size_t slot;
    std::uint32_t seq;
  };

  using slot_set = std::bitset<Section.slot_count>;

  // The newest slot whose header names this section and fits, among those
  // not tried yet.
  constexpr auto newest_untried(slot_set const& tried, bool& read_failed)
      -> std::optional<candidate>
  {
    std::optional<candidate> best;

    for (auto slot = 0uz; slot < Section.slot_count; ++slot) {
      if (tried.test(slot)) continue;

      auto const head = std::span{buffer_}.first(record_header_size);
      if (!storage_.read(address_of(slot), head)) {
        read_failed = true;
        continue;
      }

      auto const header = decode_header(head, Section.magic);
      if (!header) continue;
      if (record_size(header->count) > Section.slot_capacity) continue;

      if (!best || seq_newer(header->seq, best->seq)) {
        best = candidate{slot, header->seq};
      }
    }
    return best;
  }

  // Resumes from what the medium says, after a restart or a save before
  // the first load: the sequence continues above the number given, the
  // next record goes to the slot after the one given, and the slot ahead
  // is no longer known to be erased. The two need not come from one slot —
  // a load passes the record it restored and the newest header there is,
  // which differ after a torn save; a survey has only headers to go by.
  constexpr void adopt(std::size_t slot, std::uint32_t seq)
  {
    last_seq_ = seq;
    next_slot_ = (slot + 1) % Section.slot_count;
    surveyed_ = true;
    slot_ahead_unknown_ = true;
  }

  // Once after the chain breaks, before the next save. The slot ahead may
  // hold the debris of a save interrupted before it committed — neither a
  // record nor an erased state — and writing into it would corrupt the new
  // record rather than the old one.
  //
  // The block that slot belongs to cannot be erased: the position follows
  // the record restored, so a slot that is not the first of its block
  // shares the block with that record. The next block is taken instead,
  // which the rollover erases anyway — one erase per such incident, not
  // per boot. A slot that already starts a block needs nothing: entering
  // it erases it.
  constexpr void step_over_debris()
  {
    slot_ahead_unknown_ = false;

    if (next_slot_ % Section.slots_per_block == 0) return;
    if (slot_is_erased(next_slot_)) return;

    auto const block = next_slot_ / Section.slots_per_block;
    next_slot_ = ((block + 1) * Section.slots_per_block) % Section.slot_count;
  }

  // A slot that cannot be read counts as written: stepping past it costs a
  // block, reading over it would cost the record.
  constexpr bool slot_is_erased(std::size_t slot)
  {
    if (!storage_.read(address_of(slot), std::span{buffer_})) {
      return false;
    }
    return nvm::is_erased<Storage>(buffer_);
  }

  // Headers only: enough to continue the sequence and pick the next slot,
  // without reading or trusting any record.
  constexpr void survey()
  {
    bool ignored = false;
    auto const best = newest_untried(slot_set{}, ignored);
    if (!best) {
      next_slot_ = 0;
      last_seq_ = 0;
      surveyed_ = true;
      slot_ahead_unknown_ = true;
      return;
    }
    adopt(best->slot, best->seq);
  }

  constexpr auto read_record(std::size_t slot, bool& read_failed)
      -> std::optional<std::span<std::byte const>>
  {
    auto const head = std::span{buffer_}.first(record_header_size);
    if (!storage_.read(address_of(slot), head)) {
      read_failed = true;
      return std::nullopt;
    }

    auto const header = decode_header(head, Section.magic);
    if (!header) return std::nullopt;

    auto const stored = record_size(header->count);
    if (stored > Section.slot_capacity) return std::nullopt;

    auto const whole = std::span{buffer_}.first(stored);
    if (!storage_.read(address_of(slot), whole)) {
      read_failed = true;
      return std::nullopt;
    }
    return std::span<std::byte const>{whole};
  }
};

} // namespace settings
} // namespace emb
