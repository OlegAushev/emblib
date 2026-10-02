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

// Structure describing the place of a section on a medium: `slot_count`
// slots of `slot_capacity` bytes each, slot `i` beginning at address
// `base + i * slot_capacity`.
//
// Each save writes its record into the next slot, wrapping around after the
// last, and leaves the previous record intact. On a medium that needs
// erasing, the slots form blocks of `slots_per_block` slots, and a save that
// enters a block erases the whole block first. `store` checks the
// constraints given for the fields at compile time, unless stated otherwise.
struct placement {
  // Identifier of the section, written into the header and the footer of
  // each of its records. `load` considers only the records that carry it.
  std::uint32_t magic;
  // Address of the first slot, in bytes; a multiple of the medium's
  // `write_granularity`.
  std::size_t base;
  // Size of a slot, in bytes: at least the size of a record of the schema,
  // and a multiple of the medium's `write_granularity`. It is a constant of
  // the layout rather than the size of the record: changing it moves every
  // slot except the first away from the record stored there.
  std::size_t slot_capacity;
  // Number of slots, at least 2; all of them must lie within the medium's
  // `capacity`.
  std::size_t slot_count;
  // Number of slots in a block, the range a save erases at once on a medium
  // that needs erasing. `slot_count` must be a multiple of it. On a medium
  // that needs erasing, `slot_count` must be at least twice it, and every
  // block must cover whole erase blocks of the medium, which `store` does
  // not check.
  std::size_t slots_per_block = 1;
};

// The scoped enumeration `save_stage` defines the steps of `store::save`, in
// order, to name the one that failed in a `save_failure`.
enum class save_stage : std::uint8_t {
  // Erasing the block that the slot starts, on a medium that needs erasing.
  erase,
  // Writing the header and the cells of the record.
  body,
  // Writing the footer, which commits the record.
  commit,
  // Reading the record back and checking it.
  verify,
};

// Structure describing a failed `store::save`: the step that failed and, if
// the medium reported one, its error of type `Error`.
template<typename Error>
struct save_failure {
  save_stage stage;
  // Error the medium reported, or empty if the medium reported none but the
  // record read back fails its checks, i.e. the medium did not keep what it
  // accepted. Empty only if `stage` is `save_stage::verify`.
  std::optional<Error> cause;
};

// Structure describing the outcome of `store::load`: the report on the record
// restored, its slot, and whether the medium refused a read.
struct load_result {
  // Report on the record restored; a default `load_report`, whose `valid` is
  // `false`, if none was.
  load_report record;
  // Slot of the record restored, or empty if none was and the image holds
  // the defaults.
  std::optional<std::size_t> slot;
  // Whether the medium refused a read of a header or of a record during the
  // load, whether or not a record was restored. A slot that holds no record,
  // or one that fails its checks, does not set it.
  bool read_failed = false;
};

// The class template `store` keeps images of `Schema` as records on a medium
// of type `Storage`, in the slots that `Placement` defines. `load` restores
// an image from the newest whole record, and `save` writes an image as a new
// record into the next slot, leaving the previous record intact.
//
// A save never writes into the slot of the newest record the store knows
// of, i.e. the one the last successful save wrote or `load` restored, or,
// if there is none, the one behind the newest header, nor, on a medium that
// needs erasing, erases the block of that slot. A save that fails or is
// interrupted, or a run of them, thus leaves that record intact.
//
// `Storage::write_granularity` must divide 8. The store refers to the medium
// passed to its constructor, which must outlive it.
template<auto& Schema, nvm::some_block_storage Storage, placement Placement>
class store {
  using addr_type = typename Storage::addr_type;
  using error_type = typename Storage::error_type;

  static constexpr std::size_t count = schema_t<Schema>::count;
  static constexpr std::size_t record_bytes = record_size(count);
  static constexpr std::size_t body_bytes = record_body_size(count);
  static constexpr std::size_t block_bytes =
      Placement.slots_per_block * Placement.slot_capacity;
  static constexpr std::size_t block_count =
      Placement.slot_count / Placement.slots_per_block;

  static_assert(Placement.slot_count >= 2,
                "a store needs a second slot: a save must never be the only "
                "copy of the settings");
  static_assert(Placement.slots_per_block >= 1);
  static_assert(Placement.slot_count % Placement.slots_per_block == 0);
  static_assert(!Storage::needs_erase || block_count >= 2,
                "erasing a block must never destroy the last good record, so "
                "the slots must span at least two erase blocks");
  static_assert(record_bytes <= Placement.slot_capacity,
                "the record does not fit a slot");
  static_assert(Placement.base
                        + (Placement.slot_count * Placement.slot_capacity)
                    <= Storage::capacity,
                "the section does not fit the medium");
  static_assert(Placement.base % Storage::write_granularity == 0);
  static_assert(Placement.slot_capacity % Storage::write_granularity == 0);
  static_assert(body_bytes % Storage::write_granularity == 0
                    && record_footer_size % Storage::write_granularity == 0,
                "the medium cannot write the body and the footer separately, "
                "which is what commits a record");

  Storage& storage_;

  // Buffer the size of a slot rather than of a record, so that `load` reads a
  // record written by a schema with more parameters. Every read and the
  // encoding in `save` share it: `map_slots`, `read_record` and
  // `slot_is_erased` overwrite it.
  std::array<std::byte, Placement.slot_capacity> buffer_{};

  std::size_t next_slot_ = 0;
  std::uint32_t last_seq_ = 0;
  // Slot of the newest record known to `*this`, or empty if there is none: the
  // slot of the record that `load` restored or that a successful `save` wrote
  // or, after a `survey`, the slot of the newest header, which may have no
  // whole record behind it. It is emptied by a successful `wipe`, by a
  // `survey` that finds no header, and by a `load` that restores no record
  // and during which the medium refuses a read. `next_slot_` never names it
  // nor, on a medium that needs erasing, the first slot of its block.
  std::optional<std::size_t> kept_slot_;
  // Whether `next_slot_`, `last_seq_` and `kept_slot_` have been taken from
  // the medium, by `load`, `wipe` or `survey`; otherwise `save` calls
  // `survey` first.
  bool surveyed_ = false;

public:
  constexpr explicit store(Storage& storage) : storage_(storage) {}

  // Restores `values` from the newest whole record of the section, as
  // `decode_record` does, or fills it with the defaults if there is none.
  // Reads the header of every slot once, then the records behind the headers
  // of this section, newest first, until one passes its checks.
  //
  // Afterwards the next save continues the sequence above the newest header
  // found, even if the record behind it is not whole, and `next_slot()` is
  // the slot after the record restored, not after that header. If no record
  // is restored, `next_slot()` is the slot after that header, and the store
  // keeps it as it would keep a record; if there is no header, `sequence()`
  // and `next_slot()` return zero. If no record is restored and the medium
  // refused a read, `sequence()` and `next_slot()` return zero and the next
  // save reads the headers again, as a save before any `load` does. Returns
  // the report on the record restored, its slot, and whether the medium
  // refused a read.
  constexpr load_result load(image<Schema>& values)
  {
    load_result result;

    // One pass over the section, and the only time a load asks the medium
    // about a slot it will not read whole. Everything the loop below needs
    // to order the candidates is here, so a candidate that fails its checks
    // costs the record it read and not another pass.
    auto map = map_slots();
    result.read_failed = map.read_failed;

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
      auto const best = newest_untried(map);
      if (!best) break;
      map.tried.set(best->slot);
      if (!newest) newest = best;

      auto const stored = read_record(best->slot);
      if (!stored) {
        result.read_failed |= stored.error() == no_record::unreadable;
        continue;
      }

      auto const report = decode_record(*stored, Placement.magic, values);
      if (!report.valid) continue;

      result.record = report;
      result.slot = best->slot;
      adopt(best->slot, newest->seq);
      return result;
    }

    values.restore_defaults();
    if (result.read_failed) {
      next_slot_ = 0;
      last_seq_ = 0;
      kept_slot_.reset();
      surveyed_ = false;
    }
    else {
      survey(newest);
    }
    return result;
  }

  // Writes `values` as a new record into the next slot: on a medium that
  // needs erasing, erases the block first if the slot starts one; then
  // writes the header and the cells, then the footer, which commits the
  // record, and reads the record back to verify it. On such a medium, a slot
  // inside a block is read first, and if it is not erased, the record goes
  // to the first slot of the next block instead. If no `load`, `save` or
  // successful `wipe` came before, or the last of them was a `load` that
  // restored no record and during which the medium refused a read, the save
  // reads the header of every slot first, to continue the sequence above the
  // newest one and take the slot after it, without checking the records behind
  // them.
  //
  // The slot and the sequence number advance before the first write, whether
  // or not the save succeeds, except after an erase the medium refuses, when
  // the next save takes the same slot again. The slot skips the newest
  // record the store knows of: on two slots without erasing, a failed save
  // is retried in its own slot. A failed save may still have written a whole
  // record, which a later `load` can restore, so its sequence number is not
  // reused. If the save fails, returns a `save_failure` naming the step that
  // failed and the medium's error, if it reported one.
  constexpr std::expected<void, save_failure<error_type>>
  save(image<Schema> const& values)
  {
    // A save before the first load would otherwise start counting from one
    // and write a record that looks older than what is already stored —
    // invisible to the next load, which takes the highest sequence number.
    if (!surveyed_) survey(newest_untried(map_slots()));

    auto const slot = slot_to_write();
    auto const seq = last_seq_ + 1;
    auto const record = std::span{buffer_}.first(record_bytes);

    encode_record(record, values, Placement.magic, seq);

    // Spent whether or not the attempt succeeds. A failed save can still
    // have landed — the record wrote and only the read-back failed — and
    // reusing the number would leave two records claiming one generation,
    // where a load picks by slot order rather than by age. The one
    // exception is an erase that was refused, below.
    next_slot_ = skip_kept((slot + 1) % Placement.slot_count);
    last_seq_ = seq;

    if constexpr (Storage::needs_erase) {
      if (slot % Placement.slots_per_block == 0) {
        auto const erased = storage_.erase(address_of(slot), block_bytes);
        if (!erased) {
          // Nothing was written, so there is no debris to move past, and
          // the block still has to be erased: the slot is not spent. Moving
          // on would put the position inside a block that was never
          // cleared.
          next_slot_ = slot;
          return fail(save_stage::erase, erased.error());
        }
      }
    }

    auto const body =
        storage_.write(address_of(slot), record.first(body_bytes));
    if (!body) return fail(save_stage::body, body.error());

    auto const footer =
        storage_.write(address_of(slot, body_bytes),
                       record.subspan(body_bytes, record_footer_size));
    if (!footer) return fail(save_stage::commit, footer.error());

    if (auto const back = storage_.read(address_of(slot), record); !back) {
      return fail(save_stage::verify, back.error());
    }

    auto const header = decode_header(record, Placement.magic);
    if (!header
        || header->seq != seq
        || detail::get_u32(record, record_bytes - 4)
               != detail::crc32(record.first(record_bytes - 4))) {
      return fail(save_stage::verify);
    }

    adopt(slot, seq);
    return {};
  }

  // Erases every slot of the section, so that the next `load` finds no record
  // and restores the defaults. After a successful call, `sequence()` and
  // `next_slot()` return zero. If the medium refuses to erase a block, returns
  // its error and leaves `*this` unchanged; the blocks before that one stay
  // erased, and a later `load` restores the newest whole record left in the
  // others.
  constexpr std::expected<void, error_type> wipe()
  {
    for (auto block = 0uz; block < block_count; ++block) {
      auto const at = address_of(block * Placement.slots_per_block);
      if (auto const erased = storage_.erase(at, block_bytes); !erased) {
        return erased;
      }
    }
    next_slot_ = 0;
    last_seq_ = 0;
    kept_slot_.reset();
    surveyed_ = true;
    return {};
  }

  // Returns the sequence number that the next save continues from, i.e. the
  // next record is numbered `sequence() + 1`. Before the first `load`, `save`
  // or successful `wipe`, and after a `load` that restored no record and during
  // which the medium refused a read, that number is not known yet: returns zero
  // whatever the medium holds, and the next save first reads the header of
  // every slot and continues the sequence above the newest one, or from zero if
  // there is none.
  constexpr std::uint32_t sequence() const
  {
    return last_seq_;
  }

  // Returns the slot that the next save takes. Before the first `load`, `save`
  // or successful `wipe`, and after a `load` that restored no record and during
  // which the medium refused a read, that slot is not known yet: returns zero
  // whatever the medium holds, and the next save first reads the header of
  // every slot and takes the slot after the newest one, or slot zero if there
  // is none. On a medium that needs erasing, the save takes the first slot of
  // the next block instead if the slot it would take is neither erased nor the
  // first of its block, or of the block after it if the next block holds the
  // newest record the store knows of.
  constexpr std::size_t next_slot() const
  {
    return next_slot_;
  }

private:
  static constexpr addr_type address_of(std::size_t slot,
                                        std::size_t offset = 0)
  {
    return static_cast<addr_type>(
        Placement.base + (slot * Placement.slot_capacity) + offset);
  }

  static constexpr std::unexpected<save_failure<error_type>>
  fail(save_stage stage, std::optional<error_type> cause = std::nullopt)
  {
    return std::unexpected(save_failure<error_type>{stage, cause});
  }

  struct candidate {
    std::size_t slot;
    std::uint32_t seq;
  };

  // The scoped enumeration `no_record` defines why `read_record` found no
  // record in a slot.
  enum class no_record : std::uint8_t {
    unreadable,
    // The header does not name a record of this section that fits a slot.
    debris,
  };

  using slot_set = std::bitset<Placement.slot_count>;

  // Structure holding what one pass over the slot headers found: `seq` holds
  // the sequence number of every slot that holds a candidate, and
  // `read_failed` whether the medium refused a read. Every slot that holds no
  // candidate is in `tried` from the start, so `newest_untried` sees only
  // candidates.
  struct slot_map {
    std::array<std::uint32_t, Placement.slot_count> seq{};
    slot_set tried;
    bool read_failed = false;
  };

  // Reads the header of every slot into `buffer_` and returns the map of the
  // candidates. A slot holds a candidate if `decode_header` accepts its
  // header for `Placement.magic` and the record the header declares fits a
  // slot. A slot whose header the medium refuses to read holds none, and
  // sets `read_failed`.
  constexpr slot_map map_slots()
  {
    slot_map map;

    for (auto slot = 0uz; slot < Placement.slot_count; ++slot) {
      auto const head = std::span{buffer_}.first(record_header_size);
      if (!storage_.read(address_of(slot), head)) {
        map.read_failed = true;
        map.tried.set(slot);
        continue;
      }

      auto const header = decode_header(head, Placement.magic);
      if (!header || record_size(header->count) > Placement.slot_capacity) {
        map.tried.set(slot);
        continue;
      }

      map.seq[slot] = header->seq;
    }
    return map;
  }

  // Returns the candidate with the newest sequence number, as `seq_newer`
  // compares them, among the slots not in `map.tried`, or `std::nullopt` if
  // every slot has been tried. Equal numbers go to the lower slot. Scans for
  // the newest rather than sorting: `seq_newer` compares modulo 2^32 and is
  // not transitive over the whole range of sequence numbers.
  static constexpr std::optional<candidate> newest_untried(slot_map const& map)
  {
    std::optional<candidate> best;

    for (auto slot = 0uz; slot < Placement.slot_count; ++slot) {
      if (map.tried.test(slot)) continue;

      if (!best || seq_newer(map.seq[slot], best->seq)) {
        best = candidate{slot, map.seq[slot]};
      }
    }
    return best;
  }

  // Continues the sequence above `seq`, keeps the record in `slot` and
  // positions the next save at the slot after it, and sets `surveyed_`. The
  // two need not come from the same slot: `load` passes the slot of the
  // record it restored and the newest sequence number among the headers.
  constexpr void adopt(std::size_t slot, std::uint32_t seq)
  {
    last_seq_ = seq;
    kept_slot_ = slot;
    next_slot_ = (slot + 1) % Placement.slot_count;
    surveyed_ = true;
  }

  // Returns `slot`, unless it is `*kept_slot_` on a medium that needs no
  // erasing, or the first slot of the block of `*kept_slot_` on a medium that
  // needs erasing, whose save erases that block. Then returns the slot after
  // `slot` or, on a medium that needs erasing, the first slot of the block
  // after that block, wrapping around. On such a medium, `slot` is returned
  // unchanged if it is `*kept_slot_` but does not start its block: a save
  // reads such a slot first and writes into it only if it is erased. If
  // `kept_slot_` is empty, returns `slot`.
  constexpr std::size_t skip_kept(std::size_t slot) const
  {
    if (!kept_slot_) return slot;

    if constexpr (!Storage::needs_erase) {
      if (slot != *kept_slot_) return slot;
      return (slot + 1) % Placement.slot_count;
    }
    else {
      auto const block = slot / Placement.slots_per_block;
      if ((slot % Placement.slots_per_block != 0)
          || (block != *kept_slot_ / Placement.slots_per_block)) {
        return slot;
      }
      return ((block + 1) % block_count) * Placement.slots_per_block;
    }
  }

  // Returns the slot that the current save takes: `next_slot_`, unless the
  // medium needs erasing, `next_slot_` does not start a block, and the slot
  // does not read as erased, e.g. because it holds the debris of a save that
  // never committed. Then returns the first slot of the next block, wrapping
  // around, which the save erases on entry, or, if that block holds
  // `kept_slot_`, of the block after it. The block that holds the debris is
  // not erased instead: it can hold the record that `load` restored.
  //
  // Reads the slot into `buffer_` on every call, so it has to be called
  // before the record is encoded there.
  constexpr std::size_t slot_to_write()
  {
    if constexpr (!Storage::needs_erase) {
      return next_slot_;
    }
    else {
      if (next_slot_ % Placement.slots_per_block == 0) return next_slot_;
      if (slot_is_erased(next_slot_)) return next_slot_;

      auto const block = next_slot_ / Placement.slots_per_block;
      return skip_kept(((block + 1) * Placement.slots_per_block)
                       % Placement.slot_count);
    }
  }

  // Checks whether the whole of slot `slot` reads as erased, reading it into
  // `buffer_`. A slot that the medium refuses to read counts as not erased,
  // so a save steps past it rather than writing into it.
  constexpr bool slot_is_erased(std::size_t slot)
  {
    if (!storage_.read(address_of(slot), std::span{buffer_})) {
      return false;
    }
    return nvm::is_erased<Storage>(buffer_);
  }

  // Positions `*this` from the newest candidate among the slot headers,
  // `newest`, alone: continues the sequence above it, keeps its slot and
  // positions the next save at the slot after it, or at slot zero with the
  // sequence at zero and no slot kept if there is no candidate. Does not
  // check the record behind the header.
  constexpr void survey(std::optional<candidate> const& newest)
  {
    if (!newest) {
      next_slot_ = 0;
      last_seq_ = 0;
      kept_slot_.reset();
      surveyed_ = true;
      return;
    }
    adopt(newest->slot, newest->seq);
  }

  // Reads the header of slot `slot`, then the record of the size the header
  // declares, into `buffer_`, and returns a view of that record. Returns
  // `no_record::unreadable` if the medium refuses a read, and
  // `no_record::debris` if `decode_header` rejects the header for
  // `Placement.magic` or the record it declares does not fit a slot. Does
  // not check the footer, which `decode_record` does. The view refers to
  // `buffer_` and is valid until the next read into it.
  constexpr std::expected<std::span<std::byte const>, no_record>
  read_record(std::size_t slot)
  {
    auto const head = std::span{buffer_}.first(record_header_size);
    if (!storage_.read(address_of(slot), head)) {
      return std::unexpected(no_record::unreadable);
    }

    auto const header = decode_header(head, Placement.magic);
    if (!header) return std::unexpected(no_record::debris);

    auto const stored = record_size(header->count);
    if (stored > Placement.slot_capacity) {
      return std::unexpected(no_record::debris);
    }

    auto const whole = std::span{buffer_}.first(stored);
    if (!storage_.read(address_of(slot), whole)) {
      return std::unexpected(no_record::unreadable);
    }
    return std::span<std::byte const>{whole};
  }
};

} // namespace settings
} // namespace emb
