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
// `base + i * slot_capacity`, grouped into blocks of `slots_per_block`
// consecutive slots.
//
// The slots form a ring, slot 0 following the last. A save writes its record
// into the first slot after the newest record, in that record's block, that
// reads entirely erased; if there is none, it erases the next block and
// takes its first slot, and if the section holds no record, it erases
// block 0 and takes slot 0. A save thus writes only into erased bytes, on
// any medium, and leaves the newest record intact: it never writes into its
// slot nor erases its block. `store` checks the constraints given for the
// fields at compile time, unless stated otherwise.
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
  // Number of slots in a block, the range that `store` erases at once, on
  // any medium: on a medium that needs no erasing, an erase fills the range
  // with the medium's `erased_value`. With one slot to a block, every save
  // erases the slot it takes. `slot_count` must be a multiple of it and at
  // least twice it, and every block must cover whole erase blocks of the
  // medium, which `store` does not check.
  std::size_t slots_per_block = 1;
};

// The scoped enumeration `save_stage` defines the steps of `store::save`, in
// order, to name the one that failed in a `save_failure`.
enum class save_stage : std::uint8_t {
  // Reading every slot, before anything is written. The save fails here,
  // having written and erased nothing, with the error of the first read the
  // medium refused, or with no cause if the record would not become the
  // newest, which is possible only on a medium where an older version of
  // `store` numbered records past a header whose bit 31 had rotted.
  scan,
  // Erasing the block that the slot starts, if the slot starts one, on any
  // medium.
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
  // Error the medium reported, or empty if it reported none: at
  // `save_stage::scan` if the record would not become the newest, and at
  // `save_stage::verify` if the record read back fails its checks or differs
  // from the one written in sequence number or CRC, i.e. the medium did not
  // keep what it accepted. Empty only at those two stages.
  std::optional<Error> cause;
};

// Structure describing a successful `store::save`: the slot of the record
// written and its sequence number. The next `store::load` restores that
// record, as long as the section does not change in between and the medium
// serves every read.
struct save_result {
  std::size_t slot;
  std::uint32_t seq;
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
  // Whether the medium refused a read during the load, while reading every
  // slot or while reading a record again to decode it, whether or not a
  // record was restored. A slot that holds no record, or one that fails its
  // checks, does not set it.
  bool read_failed = false;
};

namespace detail {

// The class template `slot_ring` holds the decisions of a `store` whose section
// has `SlotCount` slots in blocks of `SlotsPerBlock`: which record is the
// newest, which slot the next save takes, whether it erases the block first,
// and how it numbers the record. They are pure functions of a `view` of the
// section, so that tests can enumerate them. `SlotCount` must be a multiple
// of `SlotsPerBlock` and at least twice it.
template<std::size_t SlotCount, std::size_t SlotsPerBlock>
struct slot_ring {
  static constexpr std::size_t block_count = SlotCount / SlotsPerBlock;

  // Structure holding what reading every slot of the section found: `erased`
  // holds the slots that read entirely erased, `whole` those that hold a
  // whole record, i.e. one that `check_record` accepts, and `seq` the
  // sequence number of the record in each slot of `whole`; in the other
  // slots, `seq` counts for nothing. A slot that holds anything else, or one
  // the medium refused to read, is in neither set.
  struct view {
    std::bitset<SlotCount> erased;
    std::bitset<SlotCount> whole;
    std::array<std::uint32_t, SlotCount> seq{};
  };

  // Structure describing a save: it writes its record into `slot`, numbered
  // `seq`, and first erases the block that `slot` starts if `erase_block` is
  // `true`.
  struct write_plan {
    std::size_t slot;
    bool erase_block;
    std::uint32_t seq;
  };

  // Returns the slot of the newest record in `v`: the whole record whose
  // sequence number is newer, as `seq_newer` compares them, than those of all
  // the whole records with another number, and of several with that number,
  // the one in the lowest slot. If no record is newer than all the others,
  // which happens only on a medium where an older version of `store`
  // numbered records past a header whose bit 31 had rotted, returns the slot
  // of the highest number taken as unsigned, again the lowest of several.
  // Returns `std::nullopt` if `v` holds no whole record.
  static constexpr std::optional<std::size_t> newest(view const& v)
  {
    std::optional<std::size_t> top;
    std::optional<std::size_t> highest;
    for (auto slot = 0uz; slot < SlotCount; ++slot) {
      if (!v.whole[slot]) continue;
      if (!top || seq_newer(v.seq[slot], v.seq[*top])) top = slot;
      if (!highest || (v.seq[slot] > v.seq[*highest])) highest = slot;
    }
    if (!top) return std::nullopt;

    for (auto slot = 0uz; slot < SlotCount; ++slot) {
      if (v.whole[slot]
          && (v.seq[slot] != v.seq[*top])
          && !seq_newer(v.seq[*top], v.seq[slot])) {
        return highest;
      }
    }
    return top;
  }

  // Returns the plan of the next save on the section that `v` describes: the
  // first slot after the newest record, in that record's block, that is in
  // `v.erased`, or, if there is none, the first slot of the next block,
  // wrapping around after the last, with `erase_block` set. The record is
  // numbered with the sequence number of the newest record plus the distance
  // between their slots, i.e. how many slots the planned one lies ahead on
  // the ring, from 1 to `SlotsPerBlock`; the sum wraps modulo 2^32. If `v`
  // holds no whole record, the plan is slot 0, with `erase_block` set, and
  // sequence number 1. A plan never takes the slot of the newest record nor
  // erases its block.
  static constexpr write_plan plan(view const& v)
  {
    auto const top = newest(v);
    auto const from = top.value_or(SlotCount - 1);
    auto const base = top ? v.seq[*top] : std::uint32_t{0};

    auto slot = from + 1;
    while ((slot % SlotsPerBlock != 0) && !v.erased[slot]) ++slot;
    auto const erase_block = (slot % SlotsPerBlock == 0);
    slot %= SlotCount;

    auto const distance = (slot + SlotCount - from) % SlotCount;
    return {.slot = slot,
            .erase_block = erase_block,
            .seq = static_cast<std::uint32_t>(base + distance)};
  }

  // Updates `v` to what the section holds once the save that `p` plans has
  // gone through: if `p.erase_block` is `true`, every slot of the block that
  // `p.slot` starts is erased and holds no record; then `p.slot` holds a
  // whole record numbered `p.seq`. `p` must be a plan that `plan` returned.
  static constexpr void apply(view& v, write_plan const& p)
  {
    if (p.erase_block) {
      for (auto slot = p.slot; slot < p.slot + SlotsPerBlock; ++slot) {
        v.erased.set(slot);
        v.whole.reset(slot);
      }
    }
    v.erased.reset(p.slot);
    v.whole.set(p.slot);
    v.seq[p.slot] = p.seq;
  }
};

} // namespace detail

// The class template `store` keeps images of `Schema` as records on a medium
// of type `Storage`, in the slots that `Placement` defines. `load` restores
// an image from the newest record, and `save` writes an image as a new
// record after it. A save never writes into the slot of the newest record
// nor erases its block, whether it succeeds or not, so a save that fails or
// is interrupted leaves that record intact.
//
// The store keeps nothing about the medium between calls: `load`, `save` and
// `wipe` each read every slot whole first and decide from what they read, so
// a save before any `load` works like any other. Each call thus reads the
// whole section, checks the CRC of every record in it and holds a table of
// `Placement.slot_count` sequence numbers on the stack. A slot holds a
// record only if `check_record` accepts it for `Placement.magic`; a header
// with no whole record behind it, such as a torn save leaves, counts for
// nothing. The newest record is the one with the newest sequence number, as
// `detail::slot_ring::newest` determines it.
//
// `Storage::write_granularity` must divide 8. The store refers to the medium
// passed to its constructor, which must outlive it.
template<auto& Schema, nvm::some_storage Storage, placement Placement>
class store {
  using addr_type = typename Storage::addr_type;
  using error_type = typename Storage::error_type;
  using slot_ring =
      detail::slot_ring<Placement.slot_count, Placement.slots_per_block>;

  static constexpr std::size_t count = schema_t<Schema>::count;
  static constexpr std::size_t record_bytes = record_size(count);
  static constexpr std::size_t body_bytes = record_body_size(count);
  static constexpr std::size_t block_bytes =
      Placement.slots_per_block * Placement.slot_capacity;

  static_assert(Placement.slots_per_block >= 1);
  static_assert(Placement.slot_count % Placement.slots_per_block == 0);
  static_assert(slot_ring::block_count >= 2,
                "a save erases the block after the one holding the newest "
                "record, never that one, so the slots must span at least two "
                "blocks");
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
  std::array<std::byte, Placement.slot_capacity> buffer_{};

public:
  constexpr explicit store(Storage& storage) : storage_(storage) {}

  // Restores `values` from the newest record of the section, as
  // `decode_record` does, or fills it with the defaults if there is none.
  // Reads every slot whole, leaving out any slot the medium refuses to read,
  // then reads the newest record again to decode it, retrying once if the
  // medium refuses. If the medium refuses that read twice, or the record
  // read fails its checks or carries another sequence number than at the
  // first read, the record is passed over and the next newest is tried.
  // Returns the report on the record restored, its slot, and whether the
  // medium refused a read.
  constexpr load_result load(image<Schema>& values)
  {
    auto [seen, refused] = scan();
    load_result result;
    result.read_failed = refused.has_value();

    while (auto const slot = slot_ring::newest(seen)) {
      auto read = read_slot(*slot);
      if (!read) {
        result.read_failed = true;
        read = read_slot(*slot);
      }
      if (read) {
        auto const report = decode_record(buffer_, Placement.magic, values);
        if (report.valid && (report.seq == seen.seq[*slot])) {
          result.record = report;
          result.slot = slot;
          return result;
        }
      }
      seen.whole.reset(*slot);
    }

    values.restore_defaults();
    return result;
  }

  // Writes `values` as a new record into the section and returns the slot
  // and the sequence number of the record, which the next `load` restores as
  // long as the section does not change in between and the medium serves
  // every read.
  //
  // Reads every slot whole first. The record goes into the first slot after
  // the newest record, in that record's block, that reads entirely erased;
  // if there is none, the save erases the next block and takes its first
  // slot, and if the section holds no record, it erases block 0 and takes
  // slot 0. The record is numbered with the sequence number of the newest
  // record plus the distance along the ring from its slot to the one taken,
  // or 1 if there is no record. If the medium refused a read, or if the
  // record, once written, would not be the newest, the save fails at
  // `save_stage::scan`, having written and erased nothing. Otherwise it
  // erases the block, if it takes the slot that starts it, writes the header
  // and the cells, then the footer, which commits the record, and reads the
  // record back, which must be whole and carry the sequence number and the
  // CRC written.
  //
  // If the save fails, returns a `save_failure` naming the step that failed
  // and the medium's error, if it reported one. A failed save may still have
  // left its record whole, e.g. if only the read-back was refused; the next
  // `load` then restores it, and the next save numbers its record past it.
  constexpr std::expected<save_result, save_failure<error_type>>
  save(image<Schema> const& values)
  {
    auto [seen, refused] = scan();
    if (refused) return fail(save_stage::scan, refused);

    auto const target = slot_ring::plan(seen);
    slot_ring::apply(seen, target);
    if (slot_ring::newest(seen) != target.slot) return fail(save_stage::scan);

    auto const record = std::span{buffer_}.first(record_bytes);
    encode_record(record, values, Placement.magic, target.seq);
    auto const crc = detail::get_u32(record, record_bytes - 4);

    if (target.erase_block) {
      auto const erased = storage_.erase(address_of(target.slot), block_bytes);
      if (!erased) return fail(save_stage::erase, erased.error());
    }

    auto const body =
        storage_.write(address_of(target.slot), record.first(body_bytes));
    if (!body) return fail(save_stage::body, body.error());

    auto const footer = storage_.write(address_of(target.slot, body_bytes),
                                       record.subspan(body_bytes));
    if (!footer) return fail(save_stage::commit, footer.error());

    if (auto const back = storage_.read(address_of(target.slot), record);
        !back) {
      return fail(save_stage::verify, back.error());
    }

    auto const stored = check_record(record, Placement.magic);
    if (!stored
        || (stored->seq != target.seq)
        || (detail::get_u32(record, record_bytes - 4) != crc)) {
      return fail(save_stage::verify);
    }

    return save_result{.slot = target.slot, .seq = target.seq};
  }

  // Erases every block of the section, so that the next `load` finds no
  // record and restores the defaults, and the next save takes slot 0 with
  // sequence number 1. Reads every slot first, only to order the erases: the
  // blocks are erased in ring order, starting after the block of the newest
  // record, so that this block comes last and a wipe cut short before its
  // last erase leaves the newest record in place. If no record is found, the
  // erases start at block 0. A read the medium refuses does not stop the
  // wipe; the order then follows the records it could read.
  //
  // If the medium refuses an erase, returns its error without erasing the
  // blocks after it; the blocks erased before it stay erased, and a later
  // `load` restores the newest record left in the others.
  constexpr std::expected<void, error_type> wipe()
  {
    auto const newest = slot_ring::newest(scan().seen);
    auto const last = newest.value_or(Placement.slot_count - 1)
                    / Placement.slots_per_block;

    for (auto i = 1uz; i <= slot_ring::block_count; ++i) {
      auto const block = (last + i) % slot_ring::block_count;
      auto const at = address_of(block * Placement.slots_per_block);
      if (auto const erased = storage_.erase(at, block_bytes); !erased) {
        return erased;
      }
    }
    return {};
  }

private:
  struct scan_result {
    typename slot_ring::view seen;
    std::optional<error_type> refused;
  };

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

  constexpr scan_result scan()
  {
    scan_result found;

    for (auto slot = 0uz; slot < Placement.slot_count; ++slot) {
      if (auto const read = read_slot(slot); !read) {
        if (!found.refused) found.refused = read.error();
        continue;
      }

      if (nvm::is_erased<Storage>(buffer_)) {
        found.seen.erased.set(slot);
      }
      else if (auto const header = check_record(buffer_, Placement.magic)) {
        found.seen.whole.set(slot);
        found.seen.seq[slot] = header->seq;
      }
    }
    return found;
  }

  constexpr nvm::result<Storage> read_slot(std::size_t slot)
  {
    return storage_.read(address_of(slot), buffer_);
  }
};

} // namespace settings
} // namespace emb
