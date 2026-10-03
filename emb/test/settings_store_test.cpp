#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include <emb/nvm/storage.hpp>
#include <emb/settings/store.hpp>
#include <emb/test/mock/block_storage.hpp>
#include <emb/units.hpp>

namespace {

using namespace emb;
using namespace emb::settings;

using rpm = units::rpm_f32;
using emb::test::storage_fault;

inline constexpr std::uint32_t magic = 0x4746434Fu; // "OCFG"

inline constexpr auto schema =
    make_schema(param("motor.p",
                      std::int32_t{11},
                      {.min = std::int32_t{1}, .max = std::int32_t{64}}),
                param("motor.R", 0.0014f, {.min = 0.0f, .max = 1.0f}),
                param("drive.phase_swap", false),
                param("drive.runout_speed",
                      rpm{100.0f},
                      {.min = rpm{0.0f}, .max = rpm{5000.0f}}));

// A later firmware, two parameters richer.
inline constexpr auto next_schema =
    make_schema(param("motor.p",
                      std::int32_t{11},
                      {.min = std::int32_t{1}, .max = std::int32_t{64}}),
                param("motor.R", 0.0014f, {.min = 0.0f, .max = 1.0f}),
                param("drive.phase_swap", false),
                param("drive.runout_speed",
                      rpm{100.0f},
                      {.min = rpm{0.0f}, .max = rpm{5000.0f}}),
                param("hall.enabled", true),
                param("hall.poll_num", std::int32_t{4}));

// FRAM: byte writes, no erase, two slots.
using fram = test::block_storage<512>;
inline constexpr placement fram_placement{.magic = magic,
                                          .base = 0,
                                          .slot_capacity = 128,
                                          .slot_count = 2};
using fram_store = store<schema, fram, fram_placement>;

// Internal flash: four-byte writes, an erased target required, two erase
// blocks of two slots each.
using flash = test::block_storage<1024, 4, true, 128>;
inline constexpr placement flash_placement{.magic = magic,
                                           .base = 0,
                                           .slot_capacity = 64,
                                           .slot_count = 4,
                                           .slots_per_block = 2};
using flash_store = store<schema, flash, flash_placement>;

using save_outcome = std::expected<save_result, save_failure<storage_fault>>;

// Whether `saved` reports a record written into `slot` and numbered `seq`.
constexpr bool saved_as(save_outcome const& saved,
                        std::size_t slot,
                        std::uint32_t seq)
{
  return saved && (saved->slot == slot) && (saved->seq == seq);
}

// Whether `saved` reports a save that failed at `stage`.
constexpr bool failed_at(save_outcome const& saved, save_stage stage)
{
  return !saved && (saved.error().stage == stage);
}

// Whether a store just started on `memory` restores the record in `slot`
// numbered `seq`.
template<typename Store, typename Memory>
constexpr bool loads(Memory& memory, std::size_t slot, std::uint32_t seq)
{
  Store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);
  return result.record.valid
      && (result.slot == slot)
      && (result.record.seq == seq);
}

// Saves `count` records in a row, the i-th, counting from one, holding i in
// "motor.p".
template<typename Store>
constexpr bool save_in_a_row(Store& store, std::size_t count)
{
  image<schema> values;
  for (auto i = 1uz; i <= count; ++i) {
    if (!values.set<"motor.p">(static_cast<std::int32_t>(i))) return false;
    if (!store.save(values)) return false;
  }
  return true;
}

// Writes a whole record of `values` numbered `seq` into `slot` of `medium`,
// as a save that went through leaves it.
constexpr void put_record(std::span<std::byte> medium,
                          placement const& where,
                          std::size_t slot,
                          std::uint32_t seq,
                          image<schema> const& values = {})
{
  auto _ = encode_record(medium.subspan(slot * where.slot_capacity),
                         values,
                         where.magic,
                         seq);
}

// Writes the body of a record numbered `seq` into `slot` of `medium` and no
// footer, as a save torn at the commit leaves it.
constexpr void put_body(std::span<std::byte> medium,
                        placement const& where,
                        std::size_t slot,
                        std::uint32_t seq)
{
  std::array<std::byte, record_size(schema.count)> record{};
  auto _ = encode_record(record, image<schema>{}, where.magic, seq);
  std::ranges::copy(std::span{record}.first(record_body_size(schema.count)),
                    medium.subspan(slot * where.slot_capacity).begin());
}

// Whether `slot` holds the same bytes on `a` and on `b`.
template<typename Memory>
constexpr bool same_slot(Memory const& a,
                         Memory const& b,
                         placement const& where,
                         std::size_t slot)
{
  auto const at = slot * where.slot_capacity;
  return std::ranges::equal(a.bytes().subspan(at, where.slot_capacity),
                            b.bytes().subspan(at, where.slot_capacity));
}

// -- An untouched medium --

consteval bool test_nothing_stored()
{
  fram memory;
  fram_store store{memory};
  image<schema> values;
  if (!values.set<"motor.p">(std::int32_t{7})) return false;

  auto const result = store.load(values);

  if (result.record.valid) return false;
  if (result.slot) return false;
  if (result.read_failed) return false;
  // Whatever the image held, it comes up defined.
  if (values.get<"motor.p">() != 11) return false;

  return true;
}

// -- Round trip --

consteval bool test_save_and_load()
{
  fram memory;
  image<schema> values;
  if (!values.set<"motor.p">(std::int32_t{4})) return false;
  if (!values.set<"drive.runout_speed">(rpm{250.0f})) return false;

  {
    fram_store store{memory};
    if (!saved_as(store.save(values), 0, 1)) return false;
  }

  // A restart: nothing is remembered but the medium.
  fram_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);

  if (!result.record.valid) return false;
  if (result.slot != 0) return false;
  if (result.record.seq != 1) return false;
  if (result.record.loaded != schema.count) return false;
  if (restored.get<"motor.p">() != 4) return false;
  if (restored.get<"drive.runout_speed">() != rpm{250.0f}) return false;
  // The next save goes to the other slot, leaving this record intact.
  if (!saved_as(restarted.save(restored), 1, 2)) return false;

  return true;
}

consteval bool test_slots_alternate()
{
  fram memory;
  fram_store store{memory};
  image<schema> values;

  if (!saved_as(store.save(values), 0, 1)) return false;
  if (!values.set<"motor.p">(std::int32_t{5})) return false;
  if (!saved_as(store.save(values), 1, 2)) return false;

  fram_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);

  if (result.slot != 1 || result.record.seq != 2) return false;
  if (restored.get<"motor.p">() != 5) return false;

  return true;
}

// -- Power loss --

consteval bool test_power_lost_while_writing_the_body()
{
  fram memory;
  image<schema> first;
  image<schema> second;
  if (!second.set<"motor.p">(std::int32_t{5})) return false;

  fram_store store{memory};
  if (!store.save(first)) return false;

  // Cut the power part way through the second record.
  memory.set_power_budget(20);
  auto const interrupted = store.save(second);
  if (interrupted) return false;
  if (interrupted.error().stage != save_stage::body) return false;

  memory.set_power_budget(fram::unlimited);

  fram_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);

  // The half-written record is not a record; the first one still stands.
  if (!result.record.valid) return false;
  if (result.slot != 0 || result.record.seq != 1) return false;
  if (restored.get<"motor.p">() != 11) return false;

  return true;
}

consteval bool test_power_lost_at_the_commit()
{
  fram memory;
  image<schema> first;
  image<schema> second;
  if (!second.set<"motor.p">(std::int32_t{5})) return false;

  fram_store store{memory};
  if (!store.save(first)) return false;

  // Enough for the whole body, nothing for the footer: the record is
  // complete on the medium except for the word that commits it.
  memory.set_power_budget(record_body_size(schema.count));
  auto const interrupted = store.save(second);
  if (interrupted) return false;
  if (interrupted.error().stage != save_stage::commit) return false;

  memory.set_power_budget(fram::unlimited);

  fram_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);

  if (!result.record.valid) return false;
  if (result.slot != 0 || result.record.seq != 1) return false;
  if (restored.get<"motor.p">() != 11) return false;

  return true;
}

// A restart after a torn save must not aim the next save at the record it
// just restored: on two slots, the slot past the debris is that record.
// Two torn saves with a restart between them would otherwise leave nothing.
consteval bool test_a_second_tear_after_a_restart()
{
  fram memory;
  image<schema> values;
  if (!values.set<"motor.p">(std::int32_t{4})) return false;

  {
    fram_store store{memory};
    if (!store.save(values)) return false; // slot 0, seq 1
    if (!values.set<"motor.p">(std::int32_t{5})) return false;
    memory.set_power_budget(record_body_size(schema.count));
    if (store.save(values)) return false; // slot 1: torn
  }
  memory.set_power_budget(fram::unlimited);

  {
    fram_store store{memory};
    image<schema> restored;
    auto const result = store.load(restored);
    if (!result.record.valid || result.slot != 0) return false;

    // The next save goes over the debris, not over the record restored.
    if (!restored.set<"motor.p">(std::int32_t{6})) return false;
    memory.set_power_budget(record_body_size(schema.count));
    if (!failed_at(store.save(restored), save_stage::commit)) return false;
  }
  memory.set_power_budget(fram::unlimited);

  fram_store restarted{memory};
  image<schema> again;
  auto const result = restarted.load(again);
  if (!result.record.valid || result.slot != 0) return false;
  if (again.get<"motor.p">() != 4) return false;

  return true;
}

consteval bool test_a_corrupted_record_falls_back_to_the_previous_one()
{
  fram memory;
  image<schema> values;
  fram_store store{memory};

  if (!store.save(values)) return false; // slot 0, seq 1
  if (!values.set<"motor.p">(std::int32_t{5})) return false;
  if (!store.save(values)) return false; // slot 1, seq 2

  // A bit rots in the newest record.
  memory.bytes()[fram_placement.slot_capacity + record_header_size + 2] ^=
      std::byte{0x08};

  fram_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);

  if (!result.record.valid) return false;
  if (result.slot != 0 || result.record.seq != 1) return false;
  if (restored.get<"motor.p">() != 11) return false;
  // The next save goes over the corrupt record, not over the one restored.
  if (!saved_as(restarted.save(restored), 1, 2)) return false;

  return true;
}

// FRAM needs no erase, yet a save fills its slot with the erased value before
// writing. A save torn there leaves its first bytes followed by the erased
// value, never the tail of the record that held the slot before.
consteval bool test_fram_fills_the_slot_before_writing()
{
  fram memory;
  fram_store store{memory};
  image<schema> values;
  if (!store.save(values)) return false; // slot 0, seq 1
  if (!store.save(values)) return false; // slot 1, seq 2

  if (!values.set<"motor.p">(std::int32_t{5})) return false;
  memory.set_power_budget(5);
  if (!failed_at(store.save(values), save_stage::body)) return false;
  memory.set_power_budget(fram::unlimited);

  std::array<std::byte, record_size(schema.count)> record{};
  auto _ = encode_record(record, values, magic, 3);
  auto const slot = memory.bytes().first(fram_placement.slot_capacity);
  if (!std::ranges::equal(slot.first(5), std::span{record}.first(5))) {
    return false;
  }
  if (!nvm::is_erased<fram>(slot.subspan(5))) return false;
  if (!loads<fram_store>(memory, 1, 2)) return false;

  return true;
}

// -- A medium that stops holding data --

consteval bool test_a_write_that_does_not_stick_is_caught()
{
  fram memory;
  image<schema> values;
  fram_store store{memory};

  memory.set_write_sink(true);
  auto const saved = store.save(values);

  if (saved) return false;
  if (saved.error().stage != save_stage::verify) return false;
  // The medium reported no error of its own — the record simply is not
  // there.
  if (saved.error().cause.has_value()) return false;

  return true;
}

// A write-protected FRAM acknowledges the fill and the write of a save and
// keeps neither. The slot still holds the whole record two saves older,
// under another sequence number: the read-back does not take it for the new
// one, and the medium stays as it was.
consteval bool test_a_write_protected_fram_is_caught()
{
  fram memory;
  fram_store store{memory};
  image<schema> values;
  if (!saved_as(store.save(values), 0, 1)) return false;
  if (!saved_as(store.save(values), 1, 2)) return false;

  if (!values.set<"motor.p">(std::int32_t{5})) return false;
  memory.set_write_sink(true);
  auto const before = memory;
  auto const saved = store.save(values); // slot 0, seq 3
  memory.set_write_sink(false);

  if (!failed_at(saved, save_stage::verify)) return false;
  if (saved.error().cause.has_value()) return false;
  if (memory.erase_calls != before.erase_calls + 1) return false;
  if (!std::ranges::equal(memory.bytes(), before.bytes())) return false;
  if (!loads<fram_store>(memory, 1, 2)) return false;

  return true;
}

consteval bool test_a_save_that_landed_but_could_not_be_read_back()
{
  flash memory;
  image<schema> values;
  flash_store store{memory};

  if (!store.save(values)) return false; // slot 0, seq 1

  // The record lands, but the medium refuses the read-back after serving
  // the reads of every slot before it.
  if (!values.set<"motor.p">(std::int32_t{2})) return false;
  memory.set_read_faults(flash_placement.slot_count, 1);
  auto const unverified = store.save(values); // slot 1, seq 2

  if (unverified) return false;
  if (unverified.error().stage != save_stage::verify) return false;
  // Here the medium did report an error of its own, unlike a write that
  // silently kept nothing.
  if (!unverified.error().cause.has_value()) return false;

  // The next save finds the record that landed and goes past it, rather
  // than reusing its slot or its sequence number.
  if (!values.set<"motor.p">(std::int32_t{3})) return false;
  if (!saved_as(store.save(values), 2, 3)) return false;

  flash_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);

  if (!result.record.valid) return false;
  if (result.slot != 2) return false;
  if (restored.get<"motor.p">() != 3) return false;

  // A medium that will not read at all gets no write: the save could not
  // tell where the newest record is.
  auto const writes = memory.write_calls;
  auto const erases = memory.erase_calls;
  memory.set_read_fault(true);
  if (!failed_at(store.save(values), save_stage::scan)) return false;
  memory.set_read_fault(false);
  if (memory.write_calls != writes || memory.erase_calls != erases) {
    return false;
  }

  return true;
}

// The record lands whole, but the read-back returns it with a bit flipped in
// a cell, under the sequence number and the CRC written. The save cannot
// vouch for the record and fails; the record itself is whole, and the next
// load restores it.
consteval bool test_a_read_back_that_differs_is_not_trusted()
{
  fram memory;
  fram_store store{memory};
  image<schema> values;
  if (!saved_as(store.save(values), 0, 1)) return false;

  if (!values.set<"motor.p">(std::int32_t{5})) return false;
  memory.corrupt_read(fram_placement.slot_count,
                      record_header_size + 4,
                      std::byte{0x08});
  auto const saved = store.save(values); // slot 1, seq 2
  if (!failed_at(saved, save_stage::verify)) return false;
  if (saved.error().cause.has_value()) return false;

  if (!loads<fram_store>(memory, 1, 2)) return false;

  return true;
}

// On two slots every retry goes over the debris of the one before it: the
// slot past the debris holds the newest record. Moving on would put the
// second tear into the record, and two in a row would leave nothing.
consteval bool test_failed_saves_in_a_row_on_two_slots()
{
  fram memory;
  image<schema> values;
  if (!values.set<"motor.p">(std::int32_t{4})) return false;

  {
    fram_store store{memory};
    if (!store.save(values)) return false; // slot 0, seq 1
    auto const kept = memory;

    if (!values.set<"motor.p">(std::int32_t{5})) return false;
    for (auto i = 0uz; i < 3; ++i) {
      memory.set_power_budget(20);
      if (store.save(values)) return false; // slot 1: torn
      if (!same_slot(memory, kept, fram_placement, 0)) return false;
    }
  }
  memory.set_power_budget(fram::unlimited);

  fram_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);
  if (!result.record.valid || result.slot != 0) return false;
  if (restored.get<"motor.p">() != 4) return false;

  return true;
}

// -- Erasable media --

consteval bool test_flash_rolls_over_between_blocks()
{
  flash memory;
  flash_store store{memory};
  image<schema> values;

  // Four saves fill both blocks: slots 0,1 then 2,3.
  for (auto i = 0uz; i < 4; ++i) {
    if (!values.set<"motor.p">(static_cast<std::int32_t>(i + 1))) return false;
    auto const saved = store.save(values);
    if (!saved_as(saved, i, static_cast<std::uint32_t>(i + 1))) return false;
  }
  if (memory.erase_calls != 2) return false;

  // The fifth wraps to slot 0 and erases the first block, whose records are
  // all older than the one in slot 3.
  if (!values.set<"motor.p">(std::int32_t{5})) return false;
  if (!saved_as(store.save(values), 0, 5)) return false;
  if (memory.erase_calls != 3) return false;

  flash_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);

  if (!result.record.valid) return false;
  if (result.slot != 0 || result.record.seq != 5) return false;
  if (restored.get<"motor.p">() != 5) return false;

  return true;
}

consteval bool test_flash_erase_never_takes_the_last_good_record()
{
  flash memory;
  flash_store store{memory};
  image<schema> values;

  for (auto i = 0uz; i < 4; ++i) {
    if (!values.set<"motor.p">(static_cast<std::int32_t>(i + 1))) return false;
    if (!store.save(values)) return false;
  }

  // The fifth save erases the first block and then loses power one byte
  // into the record.
  memory.set_power_budget(1);
  if (!values.set<"motor.p">(std::int32_t{5})) return false;
  auto const interrupted = store.save(values);
  if (interrupted) return false;
  if (interrupted.error().stage != save_stage::body) return false;
  memory.set_power_budget(flash::unlimited);

  flash_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);

  // Everything the erase took was older than the record in the other block.
  if (!result.record.valid) return false;
  if (result.slot != 3 || result.record.seq != 4) return false;
  if (restored.get<"motor.p">() != 4) return false;

  return true;
}

consteval bool test_a_restart_onto_the_debris_of_a_torn_save()
{
  flash memory;
  image<schema> values;

  {
    flash_store store{memory};
    if (!store.save(values)) return false; // slot 0, erasing its block
    if (!values.set<"motor.p">(std::int32_t{2})) return false;

    // The body of the next record lands in slot 1; the footer does not.
    memory.set_power_budget(record_body_size(schema.count));
    auto const interrupted = store.save(values);
    if (interrupted) return false;
    if (interrupted.error().stage != save_stage::commit) return false;
  }
  memory.set_power_budget(flash::unlimited);

  // After the restart the newest whole record is in slot 0, and the slot
  // after it holds the debris — written, not erased, and in the same block
  // as the record just restored, so that block cannot be erased to clean it.
  flash_store store{memory};
  image<schema> restored;
  auto const result = store.load(restored);
  if (!result.record.valid) return false;
  if (result.slot != 0) return false;

  // The save takes the next block, numbered as the record restored plus the
  // distance to the slot taken.
  if (!restored.set<"motor.p">(std::int32_t{3})) return false;
  if (!saved_as(store.save(restored), 2, 3)) return false;

  flash_store restarted{memory};
  image<schema> again;
  auto const after = restarted.load(again);
  if (!after.record.valid) return false;
  if (again.get<"motor.p">() != 3) return false;

  return true;
}

// The same restart, but the save was cut so early that the slot has no
// header to be found by. Nothing marks it as written, so the store has to
// look at the slot itself before writing into it.
consteval bool test_a_restart_onto_debris_with_no_header()
{
  flash memory;
  image<schema> values;

  {
    flash_store store{memory};
    if (!store.save(values)) return false; // slot 0, erasing its block
    if (!values.set<"motor.p">(std::int32_t{2})) return false;

    // Four bytes into slot 1: the magic landed, nothing after it did.
    memory.set_power_budget(4);
    auto const interrupted = store.save(values);
    if (interrupted) return false;
    if (interrupted.error().stage != save_stage::body) return false;
  }
  memory.set_power_budget(flash::unlimited);

  flash_store store{memory};
  image<schema> restored;
  auto const result = store.load(restored);
  if (!result.record.valid || result.slot != 0) return false;

  if (!restored.set<"motor.p">(std::int32_t{3})) return false;
  if (!store.save(restored)) return false;

  flash_store restarted{memory};
  image<schema> again;
  if (!restarted.load(again).record.valid) return false;
  if (again.get<"motor.p">() != 3) return false;

  return true;
}

// Internal flash with blocks of four slots: deep enough for a record, a hole
// and debris to share one.
using deep = test::block_storage<2048, 4, true, 256>;
inline constexpr placement deep_placement{.magic = magic,
                                          .base = 0,
                                          .slot_capacity = 64,
                                          .slot_count = 8,
                                          .slots_per_block = 4};
using deep_store = store<schema, deep, deep_placement>;

// A save the medium refuses before taking a byte leaves its slot erased, and
// the next save takes that slot again: the slot after the record in its
// block, which needs no erase.
consteval bool test_a_refused_write_is_taken_again()
{
  deep memory;
  deep_store store{memory};
  image<schema> values;
  if (!saved_as(store.save(values), 0, 1)) return false;

  memory.set_power_budget(0);
  if (!failed_at(store.save(values), save_stage::body)) return false;
  memory.set_power_budget(deep::unlimited);

  if (!saved_as(store.save(values), 1, 2)) return false;
  if (memory.erase_calls != 1) return false;

  return true;
}

// An older store spent the slot of a save the medium refused, so a medium it
// left may hold a hole between a record and the debris of the save after.
// The hole takes the next save, and the save after that steps over the
// debris to the erased slot behind it. Neither erases the block, which holds
// the newest record.
consteval bool test_a_hole_and_debris_left_by_an_older_store()
{
  deep memory;
  put_record(memory.bytes(), deep_placement, 0, 1);
  put_body(memory.bytes(), deep_placement, 2, 3);

  deep_store store{memory};
  image<schema> values;
  if (!saved_as(store.save(values), 1, 2)) return false;
  if (!saved_as(store.save(values), 3, 4)) return false;
  if (memory.erase_calls != 0) return false;
  if (!loads<deep_store>(memory, 3, 4)) return false;

  return true;
}

// A header can also lie about its age. NOR loses programmed bits upwards,
// and a lap-old record whose sequence number gained a high bit would compare
// newer than everything. The rot breaks its CRC, so it is no record and its
// number counts for nothing: the load restores the newest whole record, and
// the saves after it go on from that one.
consteval bool test_a_lap_old_header_that_rotted_newer()
{
  flash memory;
  image<schema> values;

  {
    flash_store store{memory};
    // A lap and one more: slot 0 holds seq 5 in a freshly erased block,
    // block 1 still holds seq 3 and 4.
    for (auto i = 1uz; i <= 5; ++i) {
      if (!values.set<"motor.p">(static_cast<std::int32_t>(i))) return false;
      if (!store.save(values)) return false;
    }
    if (memory.erase_calls != 3) return false;
  }

  // Bit 15 of the sequence number in slot 2: seq 3 now reads as 32771.
  memory.bytes()[2 * flash_placement.slot_capacity + 9] |= std::byte{0x80};

  flash_store store{memory};
  image<schema> restored;
  auto const result = store.load(restored);
  if (!result.record.valid || result.slot != 0) return false;
  if (restored.get<"motor.p">() != 5) return false;

  // The next save tears one byte in. Nothing may have been erased for it.
  if (!restored.set<"motor.p">(std::int32_t{6})) return false;
  memory.set_power_budget(1);
  auto const interrupted = store.save(restored);
  if (interrupted) return false;
  if (interrupted.error().stage != save_stage::body) return false;
  memory.set_power_budget(flash::unlimited);
  if (memory.erase_calls != 3) return false;

  flash_store restarted{memory};
  image<schema> again;
  auto const after = restarted.load(again);
  if (!after.record.valid || after.slot != 0) return false;
  if (again.get<"motor.p">() != 5) return false;

  // The save after it steps over the debris into the other block, erasing
  // the rotted header with it.
  if (!saved_as(restarted.save(again), 2, 7)) return false;
  if (memory.erase_calls != 4) return false;

  return true;
}

// An erase that is refused leaves its block as it was, with nothing written,
// while the newest record lives in the other block. The next save finds the
// same medium and erases the same block again, rather than moving on to the
// block it must keep.
consteval bool test_a_save_after_an_erase_that_failed()
{
  flash memory;
  flash_store store{memory};
  image<schema> values;

  // A full lap: slots 0..3, each block erased on entry.
  for (auto i = 1uz; i <= 4; ++i) {
    if (!values.set<"motor.p">(static_cast<std::int32_t>(i))) return false;
    if (!store.save(values)) return false;
  }

  // The fifth wraps to slot 0 and cannot erase its block.
  memory.set_power_budget(0);
  if (!values.set<"motor.p">(std::int32_t{5})) return false;
  auto const refused = store.save(values);
  if (refused) return false;
  if (refused.error().stage != save_stage::erase) return false;
  memory.set_power_budget(flash::unlimited);

  // The sixth erases the block and takes slot 0. The record in slot 3
  // stands throughout.
  if (!values.set<"motor.p">(std::int32_t{6})) return false;
  if (!saved_as(store.save(values), 0, 5)) return false;
  if (memory.erase_calls != 4) return false;

  flash_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);
  if (!result.record.valid || result.slot != 0) return false;
  if (restored.get<"motor.p">() != 6) return false;

  return true;
}

// The same refusal, and then the retry tears one byte in. The erase that
// went through took the block it was meant to, not the one holding the
// newest record, so that record is what comes back.
consteval bool test_a_torn_save_after_an_erase_that_failed()
{
  flash memory;
  image<schema> values;

  {
    flash_store store{memory};
    for (auto i = 1uz; i <= 4; ++i) {
      if (!values.set<"motor.p">(static_cast<std::int32_t>(i))) return false;
      if (!store.save(values)) return false;
    }

    memory.set_power_budget(0);
    if (!values.set<"motor.p">(std::int32_t{5})) return false;
    if (store.save(values)) return false; // erase refused

    memory.set_power_budget(1);
    if (!values.set<"motor.p">(std::int32_t{6})) return false;
    auto const interrupted = store.save(values);
    if (interrupted) return false;
    if (interrupted.error().stage != save_stage::body) return false;
  }
  memory.set_power_budget(flash::unlimited);

  flash_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);
  if (!result.record.valid) return false;
  if (result.slot != 3 || result.record.seq != 4) return false;
  if (restored.get<"motor.p">() != 4) return false;

  return true;
}

// An erase cut short leaves its block partly erased: here the first slot
// reads erased and the second still holds a record. The next save enters the
// block with a full erase again rather than trust the slot that reads erased.
consteval bool test_an_interrupted_erase_is_redone()
{
  flash memory;
  flash_store store{memory};
  if (!save_in_a_row(store, 4)) return false; // slots 0..3, seq 1..4

  image<schema> values;
  memory.cut_erase(0, flash_placement.slot_capacity);
  if (!failed_at(store.save(values), save_stage::erase)) return false;
  if (!loads<flash_store>(memory, 3, 4)) return false;

  auto const erases = memory.erase_calls;
  if (!saved_as(store.save(values), 0, 5)) return false;
  if (memory.erase_calls != erases + 1) return false;

  return true;
}

// A run of torn saves must stay out of the block that holds the newest
// record: entering it would erase the record. Once the debris fills the
// rest of that block, every retry takes the other block and erases it again.
consteval bool test_a_run_of_torn_saves_spares_the_block_of_the_record()
{
  flash memory;
  image<schema> values;
  if (!values.set<"motor.p">(std::int32_t{4})) return false;

  {
    flash_store store{memory};
    if (!store.save(values)) return false; // slot 0, seq 1
    auto const kept = memory;

    if (!values.set<"motor.p">(std::int32_t{5})) return false;
    for (auto i = 0uz; i < 6; ++i) {
      memory.set_power_budget(1);
      // Slots 1, 2, 2, 2, 2, 2.
      if (!failed_at(store.save(values), save_stage::body)) return false;
    }
    // The first block once, the second on each of the five entries.
    if (memory.erase_calls != 6) return false;
    if (!same_slot(memory, kept, flash_placement, 0)) return false;
  }
  memory.set_power_budget(flash::unlimited);

  flash_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);
  if (!result.record.valid || result.slot != 0) return false;
  if (restored.get<"motor.p">() != 4) return false;

  return true;
}

// The same run on a medium that will not read. A save that cannot tell where
// the newest record is writes and erases nothing, so no tear can reach it.
consteval bool test_torn_saves_on_a_medium_that_will_not_read()
{
  flash memory;
  image<schema> values;
  if (!values.set<"motor.p">(std::int32_t{4})) return false;

  {
    flash_store store{memory};
    if (!store.save(values)) return false; // slot 0, seq 1
    auto const writes = memory.write_calls;
    auto const erases = memory.erase_calls;

    if (!values.set<"motor.p">(std::int32_t{5})) return false;
    memory.set_read_fault(true);
    for (auto i = 0uz; i < 2; ++i) {
      memory.set_power_budget(1);
      if (!failed_at(store.save(values), save_stage::scan)) return false;
    }
    memory.set_read_fault(false);
    if (memory.write_calls != writes) return false;
    if (memory.erase_calls != erases) return false;
  }
  memory.set_power_budget(flash::unlimited);

  flash_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);
  if (!result.record.valid || result.slot != 0) return false;
  if (restored.get<"motor.p">() != 4) return false;

  return true;
}

// Whatever a save runs into, the newest record keeps every byte and loads: a
// write refused before its first byte, a tear anywhere in the record, a write
// the medium drops, an erase refused or cut short, a medium that will not
// read. Only a tear past which every byte of the new record equals the
// erased value leaves that record whole, and then it loads instead.
consteval bool test_the_newest_record_is_never_touched()
{
  flash memory;
  flash_store store{memory};
  image<schema> values;
  if (!values.set<"motor.p">(std::int32_t{4})) return false;
  if (!store.save(values)) return false; // slot 0, seq 1
  auto const kept = memory;

  auto const untouched = [&] {
    image<schema> restored;
    auto const result = flash_store{memory}.load(restored);
    return same_slot(memory, kept, flash_placement, 0)
        && (result.slot == 0)
        && (result.record.seq == 1)
        && (restored.get<"motor.p">() == 4);
  };

  if (!values.set<"motor.p">(std::int32_t{5})) return false;

  memory.set_power_budget(0);
  if (!failed_at(store.save(values), save_stage::body)) return false;
  memory.set_power_budget(flash::unlimited);
  if (!untouched()) return false;

  for (auto const tear : {1uz, 9uz, 20uz, 47uz}) {
    memory.set_power_budget(tear);
    if (!failed_at(store.save(values), save_stage::body)) return false;
    memory.set_power_budget(flash::unlimited);
    if (!untouched()) return false;
  }

  memory.set_write_sink(true);
  if (!failed_at(store.save(values), save_stage::verify)) return false;
  memory.set_write_sink(false);
  if (!untouched()) return false;

  memory.set_power_budget(0);
  if (!failed_at(store.save(values), save_stage::erase)) return false;
  memory.set_power_budget(flash::unlimited);
  if (!untouched()) return false;

  memory.cut_erase(0, flash_placement.slot_capacity);
  if (!failed_at(store.save(values), save_stage::erase)) return false;
  if (!untouched()) return false;

  memory.set_read_fault(true);
  if (!failed_at(store.save(values), save_stage::scan)) return false;
  memory.set_read_fault(false);
  if (!untouched()) return false;

  std::array<std::byte, record_size(schema.count)> record{};
  auto _ = encode_record(record, values, magic, 3); // slot 2, seq 3
  for (auto const tear : {48uz, 52uz, 55uz}) {
    memory.set_power_budget(tear);
    if (!failed_at(store.save(values), save_stage::commit)) return false;
    memory.set_power_budget(flash::unlimited);
    if (nvm::is_erased<flash>(std::span{record}.subspan(tear))) {
      return same_slot(memory, kept, flash_placement, 0)
          && loads<flash_store>(memory, 2, 3);
    }
    if (!untouched()) return false;
  }

  return true;
}

consteval bool test_flash_round_trip()
{
  flash memory;
  image<schema> values;
  if (!values.set<"motor.R">(0.05f)) return false;

  flash_store store{memory};
  if (!store.save(values)) return false;
  if (memory.erase_calls != 1) return false;

  flash_store restarted{memory};
  image<schema> restored;
  if (!restarted.load(restored).record.valid) return false;
  if (restored.get<"motor.R">() != 0.05f) return false;

  return true;
}

// A section with more slots than a word has bits: what a 128 KiB erase
// block looks like when slots are a kilobyte.
using wide = test::block_storage<4096, 4, true, 1024>;
inline constexpr placement wide_placement{.magic = magic,
                                          .base = 0,
                                          .slot_capacity = 64,
                                          .slot_count = 64,
                                          .slots_per_block = 16};
using wide_store = store<schema, wide, wide_placement>;

consteval bool test_more_slots_than_a_word_has_bits()
{
  // Seventy saves, past one full lap, so the newest record sits in a slot
  // the search must reach after wrapping: seq 65..70 in slots 0..5 of a
  // block erased for them, seq 17..64 in slots 16..63.
  wide memory;
  image<schema> values;
  for (auto seq = std::uint32_t{17}; seq <= 70; ++seq) {
    if (!values.set<"motor.p">(static_cast<std::int32_t>(seq - 16))) {
      return false;
    }
    auto const slot =
        static_cast<std::size_t>(seq - 1) % wide_placement.slot_count;
    put_record(memory.bytes(), wide_placement, slot, seq, values);
  }

  wide_store store{memory};
  image<schema> restored;
  auto const result = store.load(restored);

  if (!result.record.valid) return false;
  if (result.slot != 5 || result.record.seq != 70) return false;
  if (restored.get<"motor.p">() != 54) return false;
  if (!saved_as(store.save(restored), 6, 71)) return false;

  return true;
}

// Every slot is read once and every record in it checked as it is read, so a
// run of corrupt records costs nothing more: one read per slot, then one for
// the record restored.
consteval bool test_a_run_of_corrupt_records_costs_one_pass()
{
  wide memory;
  wide_store store{memory};
  image<schema> values;

  for (auto i = 1uz; i <= 10; ++i) {
    if (!values.set<"motor.p">(static_cast<std::int32_t>(i))) return false;
    if (!store.save(values)) return false; // slot i - 1, seq i
  }

  // The five newest records lose their crc.
  for (auto slot = 5uz; slot <= 9uz; ++slot) {
    auto const crc = (slot * wide_placement.slot_capacity)
                   + record_size(schema_t<schema>::count)
                   - 1;
    memory.bytes()[crc] ^= std::byte{0xFF};
  }

  wide_store restarted{memory};
  image<schema> restored;
  memory.read_calls = 0;
  auto const result = restarted.load(restored);

  if (!result.record.valid) return false;
  if (result.slot != 4 || result.record.seq != 5) return false;
  if (restored.get<"motor.p">() != 5) return false;
  if (memory.read_calls != wide_placement.slot_count + 1) return false;

  // The next save steps over the corrupt records to the first erased slot.
  if (!saved_as(restarted.save(restored), 10, 11)) return false;

  return true;
}

// Headers with no whole record behind them count for nothing: the load
// restores no record, and the next save numbers from nothing, as on an empty
// medium, taking slot 0 with sequence number one.
consteval bool test_no_whole_record_behind_the_headers()
{
  fram memory;
  image<schema> values;

  {
    fram_store store{memory};
    if (!store.save(values)) return false; // slot 0, seq 1
    if (!store.save(values)) return false; // slot 1, seq 2
  }

  // Both records lose their crc.
  for (auto slot = 0uz; slot < fram_placement.slot_count; ++slot) {
    auto const crc = (slot * fram_placement.slot_capacity)
                   + record_size(schema_t<schema>::count)
                   - 1;
    memory.bytes()[crc] ^= std::byte{0xFF};
  }

  fram_store store{memory};
  image<schema> restored;
  auto const result = store.load(restored);
  if (result.record.valid || result.read_failed) return false;

  if (!restored.set<"motor.p">(std::int32_t{7})) return false;
  if (!saved_as(store.save(restored), 0, 1)) return false;

  fram_store restarted{memory};
  image<schema> again;
  auto const after = restarted.load(again);
  if (!after.record.valid || after.record.seq != 1) return false;
  if (again.get<"motor.p">() != 7) return false;

  return true;
}

// -- A medium that will not read --

// A load the medium would not serve leaves nothing behind for the save after
// it, which reads every slot itself. Counting from one again would number the
// new record below the ones the load could not read, and once the medium
// reads again, the next load would restore an older record instead.
consteval bool test_a_load_that_could_not_read()
{
  fram memory;
  image<schema> values;

  {
    fram_store store{memory};
    if (!store.save(values)) return false; // slot 0, seq 1
    if (!values.set<"motor.p">(std::int32_t{5})) return false;
    if (!store.save(values)) return false; // slot 1, seq 2
  }

  fram_store store{memory};
  image<schema> restored;
  memory.set_read_fault(true);
  auto const result = store.load(restored);
  memory.set_read_fault(false);
  if (result.record.valid || !result.read_failed) return false;
  if (restored.get<"motor.p">() != 11) return false;

  if (!restored.set<"motor.p">(std::int32_t{6})) return false;
  if (!saved_as(store.save(restored), 0, 3)) return false;

  fram_store restarted{memory};
  image<schema> again;
  auto const after = restarted.load(again);
  if (!after.record.valid || after.slot != 0) return false;
  if (again.get<"motor.p">() != 6) return false;

  return true;
}

// A save that cannot read the medium cannot tell where the newest record is,
// and a write could land on it. It writes nothing and erases nothing.
template<typename Memory, typename Store>
consteval bool a_save_that_cannot_read_writes_nothing()
{
  Memory memory;
  Store store{memory};
  image<schema> values;
  if (!store.save(values)) return false;
  if (!store.save(values)) return false;

  memory.set_read_fault(true);
  auto const before = memory;
  auto const blind = store.save(values);

  if (blind) return false;
  if (blind.error().stage != save_stage::scan) return false;
  if (blind.error().cause != storage_fault::unreadable) return false;
  if (memory.write_calls != before.write_calls) return false;
  if (memory.erase_calls != before.erase_calls) return false;
  if (!std::ranges::equal(memory.bytes(), before.bytes())) return false;

  return true;
}

consteval bool test_a_save_that_cannot_read_writes_nothing()
{
  return a_save_that_cannot_read_writes_nothing<fram, fram_store>()
      && a_save_that_cannot_read_writes_nothing<flash, flash_store>();
}

// One slot that will not read hides whatever it holds. The load restores the
// newest record among the others and reports the failure. A save writes
// nothing until the slot reads again, since it may hold the newest record.
consteval bool test_one_unreadable_slot()
{
  flash memory;
  flash_store store{memory};
  if (!save_in_a_row(store, 3)) return false; // slots 0..2, seq 1..3

  memory.set_unreadable(2 * flash_placement.slot_capacity,
                        flash_placement.slot_capacity);

  image<schema> restored;
  auto const result = store.load(restored);
  if (!result.record.valid || !result.read_failed) return false;
  if (result.slot != 1 || result.record.seq != 2) return false;
  if (restored.get<"motor.p">() != 2) return false;

  auto const refused = store.save(restored);
  if (refused) return false;
  if (refused.error().stage != save_stage::scan) return false;
  if (refused.error().cause != storage_fault::unreadable) return false;

  memory.set_unreadable(0, 0);
  if (!saved_as(store.save(restored), 3, 4)) return false;

  return true;
}

// The load reads the newest record a second time to decode it. A refusal
// there is retried once, and a second one gives that record up for the next
// newest. Either way the load reports the failure.
consteval bool test_a_second_read_that_fails()
{
  flash memory;
  flash_store store{memory};
  if (!save_in_a_row(store, 3)) return false; // slots 0..2, seq 1..3

  image<schema> restored;
  memory.set_read_faults(flash_placement.slot_count, 1);
  auto const retried = store.load(restored);
  if (!retried.record.valid || !retried.read_failed) return false;
  if (retried.slot != 2 || retried.record.seq != 3) return false;
  if (restored.get<"motor.p">() != 3) return false;

  memory.set_read_faults(flash_placement.slot_count, 2);
  auto const given_up = store.load(restored);
  if (!given_up.record.valid || !given_up.read_failed) return false;
  if (given_up.slot != 1 || given_up.record.seq != 2) return false;
  if (restored.get<"motor.p">() != 2) return false;

  return true;
}

// A bit that flips in the second read of the newest record, with no error
// from the medium, fails the record's checks. The load passes it over for
// the next newest, and reports no failure to read.
consteval bool test_a_second_read_that_differs()
{
  flash memory;
  flash_store store{memory};
  if (!save_in_a_row(store, 3)) return false; // slots 0..2, seq 1..3

  image<schema> restored;
  memory.corrupt_read(flash_placement.slot_count,
                      record_header_size + 4,
                      std::byte{0x08});
  auto const result = store.load(restored);
  if (!result.record.valid || result.read_failed) return false;
  if (result.slot != 1 || result.record.seq != 2) return false;
  if (restored.get<"motor.p">() != 2) return false;

  return true;
}

// -- Sequence numbers --

// The first save on an empty medium, torn nine bytes in, leaves a header
// whose sequence number reads 0xFFFFFF01. Numbered above it, the next record
// would run the counter through the wrap. With no record behind it the
// header counts for nothing, and the next save is numbered one.
consteval bool test_a_torn_sequence_field_counts_for_nothing()
{
  flash memory;
  image<schema> values;

  {
    flash_store store{memory};
    memory.set_power_budget(9);
    if (!failed_at(store.save(values), save_stage::body)) return false;
  }
  memory.set_power_budget(flash::unlimited);

  flash_store store{memory};
  image<schema> restored;
  auto const result = store.load(restored);
  if (result.record.valid || result.slot) return false;

  if (!saved_as(store.save(restored), 0, 1)) return false;

  return true;
}

// The counter wraps after 2^32 saves: zero and one are newer than
// 0xFFFFFFFF, and the record numbered one is the newest.
consteval bool test_a_counter_that_wrapped_still_loads()
{
  flash memory;
  put_record(memory.bytes(), flash_placement, 0, 0xFFFFFFFEu);
  put_record(memory.bytes(), flash_placement, 1, 0xFFFFFFFFu);
  put_record(memory.bytes(), flash_placement, 2, 0);
  put_record(memory.bytes(), flash_placement, 3, 1);

  if (!loads<flash_store>(memory, 3, 1)) return false;

  flash_store store{memory};
  image<schema> values;
  if (!saved_as(store.save(values), 0, 2)) return false;

  return true;
}

// Debris may carry any number in its header, the highest there is included.
// Only whole records count: the load restores the record in slot 0, and the
// next save is numbered from it and the distance to the slot it takes.
consteval bool test_debris_numbers_are_ignored()
{
  deep memory;
  put_record(memory.bytes(), deep_placement, 0, 5);
  put_body(memory.bytes(), deep_placement, 1, 0x7FFFFFFFu);
  put_body(memory.bytes(), deep_placement, 2, 0xFFFFFFFFu);

  if (!loads<deep_store>(memory, 0, 5)) return false;

  deep_store store{memory};
  image<schema> values;
  if (!saved_as(store.save(values), 3, 8)) return false;

  return true;
}

// A medium an older store left behind: bit 31 of seq 5 rotted, that store
// took the header for the newest, and numbered the next record 0x80000006,
// which is 2^31 from the record numbered 6. Neither is newer than the other,
// and the load takes the highest number: the record saved last. Each save
// after it is the record the next load restores, up to the one that erases
// the block of the rotted header.
consteval bool test_a_rotted_top_bit_the_old_store_numbered_above()
{
  deep memory;
  put_record(memory.bytes(), deep_placement, 0, 9);
  put_record(memory.bytes(), deep_placement, 1, 0x80000006u);
  put_record(memory.bytes(), deep_placement, 4, 5);
  memory.bytes()[4 * deep_placement.slot_capacity + 11] |= std::byte{0x80};
  put_record(memory.bytes(), deep_placement, 5, 6);
  put_record(memory.bytes(), deep_placement, 6, 7);
  put_record(memory.bytes(), deep_placement, 7, 8);

  if (!loads<deep_store>(memory, 1, 0x80000006u)) return false;

  deep_store store{memory};
  image<schema> values;
  if (!saved_as(store.save(values), 2, 0x80000007u)) return false;
  if (!loads<deep_store>(memory, 2, 0x80000007u)) return false;
  if (!saved_as(store.save(values), 3, 0x80000008u)) return false;
  if (!loads<deep_store>(memory, 3, 0x80000008u)) return false;
  if (memory.erase_calls != 0) return false;
  if (!saved_as(store.save(values), 4, 0x80000009u)) return false;
  if (!loads<deep_store>(memory, 4, 0x80000009u)) return false;
  if (memory.erase_calls != 1) return false;

  return true;
}

// On a medium where an older store numbered records past a rotted bit 31,
// the records in the block a save erases can decide which record is the
// newest, and the save counts them gone before it writes. Once 0x80000003
// goes with its block, the record after seq 2 is the newest, and the save
// goes through. Once 3 goes with its block, the record numbered 1 is newer
// than the rest, the new one included, and the save refuses, writing
// nothing.
consteval bool test_records_in_an_erased_block_do_not_count()
{
  {
    flash memory;
    put_record(memory.bytes(), flash_placement, 1, 2);
    put_record(memory.bytes(), flash_placement, 3, 0x80000003u);

    flash_store store{memory};
    image<schema> values;
    if (!saved_as(store.save(values), 2, 3)) return false;
    if (!loads<flash_store>(memory, 2, 3)) return false;
  }

  flash memory;
  put_record(memory.bytes(), flash_placement, 1, 3);
  put_record(memory.bytes(), flash_placement, 2, 1);
  put_record(memory.bytes(), flash_placement, 3, 0x80000003u);

  flash_store store{memory};
  image<schema> values;
  auto const refused = store.save(values);
  if (!failed_at(refused, save_stage::scan)) return false;
  if (refused.error().cause.has_value()) return false;
  if (memory.write_calls != 0 || memory.erase_calls != 0) return false;

  return true;
}

// Two records 2^31 apart: neither is newer than the other, and no number is
// newer than both. The load takes the higher one. A save could write no
// record the next load would restore, so it refuses before writing anything;
// a wipe clears the way.
consteval bool test_records_too_far_apart_refuse_the_save()
{
  flash memory;
  put_record(memory.bytes(), flash_placement, 0, 0xFFFFFFFFu);
  put_record(memory.bytes(), flash_placement, 1, 0x7FFFFFFFu);

  if (!loads<flash_store>(memory, 0, 0xFFFFFFFFu)) return false;

  flash_store store{memory};
  image<schema> values;
  auto const refused = store.save(values);
  if (refused) return false;
  if (refused.error().stage != save_stage::scan) return false;
  if (refused.error().cause.has_value()) return false;
  if (memory.write_calls != 0 || memory.erase_calls != 0) return false;

  if (!store.wipe()) return false;
  if (!saved_as(store.save(values), 0, 1)) return false;

  return true;
}

// -- Wipe --

consteval bool test_wipe()
{
  fram memory;
  image<schema> values;
  if (!values.set<"motor.p">(std::int32_t{5})) return false;

  fram_store store{memory};
  if (!store.save(values)) return false;
  if (!store.wipe()) return false;

  fram_store restarted{memory};
  image<schema> restored;
  if (restarted.load(restored).record.valid) return false;
  if (restored.get<"motor.p">() != 11) return false;

  return true;
}

// A wipe erases the block of the newest record last. Cut short, it leaves
// that record or nothing at all, never an older record to load in its place.
consteval bool test_an_interrupted_wipe_keeps_the_newest()
{
  {
    flash memory;
    flash_store store{memory};
    if (!save_in_a_row(store, 3)) return false; // the newest in slot 2
    memory.cut_erase(1, 0);
    if (store.wipe()) return false;
    if (!loads<flash_store>(memory, 2, 3)) return false;
  }
  {
    flash memory;
    flash_store store{memory};
    if (!save_in_a_row(store, 5)) return false; // the newest in slot 0
    memory.cut_erase(1, 0);
    if (store.wipe()) return false;
    if (!loads<flash_store>(memory, 0, 5)) return false;
  }

  flash memory;
  flash_store store{memory};
  if (!save_in_a_row(store, 5)) return false;
  memory.cut_erase(1, flash_placement.slot_capacity);
  if (store.wipe()) return false;

  image<schema> restored;
  auto const result = store.load(restored);
  if (result.record.valid || result.slot || result.read_failed) return false;

  return true;
}

// A wipe reads only to put the block of the newest record last. A medium that
// will not read is erased all the same.
consteval bool test_a_wipe_that_cannot_read_still_erases()
{
  flash memory;
  flash_store store{memory};
  if (!save_in_a_row(store, 3)) return false;

  memory.set_read_fault(true);
  if (!store.wipe()) return false;
  memory.set_read_fault(false);

  auto const section = memory.bytes().first(flash_placement.slot_count
                                            * flash_placement.slot_capacity);
  if (!nvm::is_erased<flash>(section)) return false;

  return true;
}

// After a wipe there is nothing to count from: the next save takes slot 0,
// numbered one, and erases its own block and nothing else.
consteval bool test_a_save_after_a_wipe_starts_at_one()
{
  flash memory;
  flash_store store{memory};
  if (!save_in_a_row(store, 3)) return false;
  if (!store.wipe()) return false;

  auto const erases = memory.erase_calls;
  image<schema> values;
  if (!saved_as(store.save(values), 0, 1)) return false;
  if (memory.erase_calls != erases + 1) return false;

  return true;
}

consteval bool test_saving_before_loading_keeps_the_sequence()
{
  fram memory;
  image<schema> values;

  {
    fram_store store{memory};
    if (!store.save(values)) return false; // slot 0, seq 1
    if (!store.save(values)) return false; // slot 1, seq 2
  }

  // A restart that saves without loading first. Counting from one again
  // would put the new record in slot 0 with a sequence number below the one
  // in slot 1, and the next load would restore the older record instead.
  fram_store store{memory};
  if (!values.set<"motor.p">(std::int32_t{9})) return false;
  if (!store.save(values)) return false;

  fram_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);

  if (!result.record.valid) return false;
  if (result.record.seq != 3) return false;
  if (restored.get<"motor.p">() != 9) return false;

  return true;
}

// -- Downgrade --

consteval bool test_a_record_written_by_a_richer_firmware_still_loads()
{
  fram memory;

  {
    image<next_schema> values;
    if (!values.set<"motor.p">(std::int32_t{6})) return false;
    if (!values.set<"hall.poll_num">(std::int32_t{2})) return false;

    store<next_schema, fram, fram_placement> newer{memory};
    if (!newer.save(values)) return false;
  }

  // The older firmware reads a record longer than any it would write: the
  // buffer is a slot, not a record.
  fram_store older{memory};
  image<schema> restored;
  auto const result = older.load(restored);

  if (!result.record.valid) return false;
  if (result.record.schema_matched) return false;
  if (result.record.stored != next_schema.count) return false;
  if (result.record.loaded != schema.count) return false;
  if (result.record.unknown != 2) return false;
  if (restored.get<"motor.p">() != 6) return false;

  return true;
}

static_assert(test_nothing_stored());
static_assert(test_save_and_load());
static_assert(test_slots_alternate());
static_assert(test_power_lost_while_writing_the_body());
static_assert(test_power_lost_at_the_commit());
static_assert(test_a_second_tear_after_a_restart());
static_assert(test_a_corrupted_record_falls_back_to_the_previous_one());
static_assert(test_fram_fills_the_slot_before_writing());
static_assert(test_a_write_that_does_not_stick_is_caught());
static_assert(test_a_write_protected_fram_is_caught());
static_assert(test_a_save_that_landed_but_could_not_be_read_back());
static_assert(test_a_read_back_that_differs_is_not_trusted());
static_assert(test_failed_saves_in_a_row_on_two_slots());
static_assert(test_flash_rolls_over_between_blocks());
static_assert(test_flash_erase_never_takes_the_last_good_record());
static_assert(test_a_restart_onto_the_debris_of_a_torn_save());
static_assert(test_a_restart_onto_debris_with_no_header());
static_assert(test_a_refused_write_is_taken_again());
static_assert(test_a_hole_and_debris_left_by_an_older_store());
static_assert(test_a_lap_old_header_that_rotted_newer());
static_assert(test_a_save_after_an_erase_that_failed());
static_assert(test_a_torn_save_after_an_erase_that_failed());
static_assert(test_an_interrupted_erase_is_redone());
static_assert(test_a_run_of_torn_saves_spares_the_block_of_the_record());
static_assert(test_torn_saves_on_a_medium_that_will_not_read());
static_assert(test_the_newest_record_is_never_touched());
static_assert(test_flash_round_trip());
static_assert(test_more_slots_than_a_word_has_bits());
static_assert(test_a_run_of_corrupt_records_costs_one_pass());
static_assert(test_no_whole_record_behind_the_headers());
static_assert(test_a_load_that_could_not_read());
static_assert(test_a_save_that_cannot_read_writes_nothing());
static_assert(test_one_unreadable_slot());
static_assert(test_a_second_read_that_fails());
static_assert(test_a_second_read_that_differs());
static_assert(test_a_torn_sequence_field_counts_for_nothing());
static_assert(test_a_counter_that_wrapped_still_loads());
static_assert(test_debris_numbers_are_ignored());
static_assert(test_a_rotted_top_bit_the_old_store_numbered_above());
static_assert(test_records_in_an_erased_block_do_not_count());
static_assert(test_records_too_far_apart_refuse_the_save());
static_assert(test_wipe());
static_assert(test_an_interrupted_wipe_keeps_the_newest());
static_assert(test_a_wipe_that_cannot_read_still_erases());
static_assert(test_a_save_after_a_wipe_starts_at_one());
static_assert(test_saving_before_loading_keeps_the_sequence());
static_assert(test_a_record_written_by_a_richer_firmware_still_loads());

} // namespace
