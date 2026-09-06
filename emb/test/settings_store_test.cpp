#include <array>
#include <cstdint>

#include <emb/settings/store.hpp>
#include <emb/test/mock/block_storage.hpp>
#include <emb/units.hpp>

namespace {

using namespace emb;
using namespace emb::settings;

using rpm = units::rpm_f32;
using emb::test::storage_fault;

inline constexpr std::uint32_t magic = 0x4746434Fu; // "OCFG"

inline constexpr auto schema = make_schema(
    param("motor.p",
          std::int32_t{11},
          {.min = std::int32_t{1}, .max = std::int32_t{64}}),
    param("motor.R", 0.0014f, {.min = 0.0f, .max = 1.0f}),
    param("drive.phase_swap", false),
    param("drive.runout_speed",
          rpm{100.0f},
          {.min = rpm{0.0f}, .max = rpm{5000.0f}}));

// A later firmware, two parameters richer.
inline constexpr auto next_schema = make_schema(
    param("motor.p",
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
inline constexpr section fram_section{.magic = magic,
                                      .base = 0,
                                      .slot_capacity = 128,
                                      .slot_count = 2};
using fram_store = store<schema, fram, fram_section>;

// Internal flash: four-byte writes, an erased target required, two erase
// blocks of two slots each.
using flash = test::block_storage<1024, 4, true, 128>;
inline constexpr section flash_section{.magic = magic,
                                       .base = 0,
                                       .slot_capacity = 64,
                                       .slot_count = 4,
                                       .slots_per_block = 2};
using flash_store = store<schema, flash, flash_section>;

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
    if (!store.save(values)) return false;
    if (store.sequence() != 1) return false;
    if (store.next_slot() != 1) return false;
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
  if (restarted.next_slot() != 1) return false;

  return true;
}

consteval bool test_slots_alternate()
{
  fram memory;
  fram_store store{memory};
  image<schema> values;

  if (!store.save(values)) return false;              // slot 0, seq 1
  if (!values.set<"motor.p">(std::int32_t{5})) return false;
  if (!store.save(values)) return false;              // slot 1, seq 2
  if (store.next_slot() != 0) return false;

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
    if (!store.save(values)) return false;                    // slot 0, seq 1
    if (!values.set<"motor.p">(std::int32_t{5})) return false;
    memory.set_power_budget(record_body_size(schema.count));
    if (store.save(values)) return false;                     // slot 1: torn
  }
  memory.set_power_budget(fram::unlimited);

  {
    fram_store store{memory};
    image<schema> restored;
    auto const result = store.load(restored);
    if (!result.record.valid || result.slot != 0) return false;
    // The next save goes over the debris, not over the record restored.
    if (store.next_slot() != 1) return false;

    if (!restored.set<"motor.p">(std::int32_t{6})) return false;
    memory.set_power_budget(record_body_size(schema.count));
    if (store.save(restored)) return false;                   // torn again
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

  if (!store.save(values)) return false;               // slot 0, seq 1
  if (!values.set<"motor.p">(std::int32_t{5})) return false;
  if (!store.save(values)) return false;               // slot 1, seq 2

  // A bit rots in the newest record.
  memory.bytes()[fram_section.slot_capacity + record_header_size + 2] ^=
      std::byte{0x08};

  fram_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);

  if (!result.record.valid) return false;
  if (result.slot != 0 || result.record.seq != 1) return false;
  if (restored.get<"motor.p">() != 11) return false;
  // The next save goes over the corrupt record, not over the one restored.
  if (restarted.next_slot() != 1) return false;

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

consteval bool test_a_save_that_landed_but_could_not_be_read_back()
{
  fram memory;
  image<schema> values;
  fram_store store{memory};

  if (!store.save(values)) return false;                     // slot 0, seq 1
  if (!values.set<"motor.p">(std::int32_t{2})) return false;
  if (!store.save(values)) return false;                     // slot 1, seq 2

  // The record lands, but the read-back cannot be performed: the medium
  // holds a third generation the store does not know about.
  if (!values.set<"motor.p">(std::int32_t{3})) return false;
  memory.set_read_fault(true);
  auto const unverified = store.save(values);                // slot 0, seq 3
  memory.set_read_fault(false);

  if (unverified) return false;
  if (unverified.error().stage != save_stage::verify) return false;
  // Here the medium did report an error of its own, unlike a write that
  // silently kept nothing.
  if (!unverified.error().cause.has_value()) return false;

  // The next save must not reuse that sequence number: two records claiming
  // one generation are ordered by slot, not by age, and this one would be
  // shadowed by the record above it.
  if (!values.set<"motor.p">(std::int32_t{4})) return false;
  if (!store.save(values)) return false;                     // slot 1, seq 4

  fram_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);

  if (!result.record.valid) return false;
  if (result.slot != 1) return false;
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
    if (!store.save(values)) return false;
  }
  if (store.next_slot() != 0) return false;
  if (memory.erase_calls != 2) return false;

  // The fifth wraps to slot 0 and erases the first block, whose records are
  // all older than the one in slot 3.
  if (!values.set<"motor.p">(std::int32_t{5})) return false;
  if (!store.save(values)) return false;
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
    if (!store.save(values)) return false;   // slot 0, erasing its block
    if (!values.set<"motor.p">(std::int32_t{2})) return false;

    // The body of the next record lands in slot 1; the footer does not.
    memory.set_power_budget(record_body_size(schema.count));
    auto const interrupted = store.save(values);
    if (interrupted) return false;
    if (interrupted.error().stage != save_stage::commit) return false;
  }
  memory.set_power_budget(flash::unlimited);

  // After the restart the newest whole record is in slot 0, and the slot
  // the store would write next holds the debris — written, not erased, and
  // in the same block as the record just restored, so that block cannot be
  // erased to clean it.
  flash_store store{memory};
  image<schema> restored;
  auto const result = store.load(restored);
  if (!result.record.valid) return false;
  if (result.slot != 0) return false;

  if (!restored.set<"motor.p">(std::int32_t{3})) return false;
  if (!store.save(restored)) return false;

  // The debris carries a header claiming generation two. The record just
  // written must be numbered above it, or the two would claim one
  // generation and a load would have to pick between them by slot order.
  if (store.sequence() != 3) return false;

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
    if (!store.save(values)) return false;   // slot 0, erasing its block
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

// A header can also lie about its age. NOR loses programmed bits upwards,
// and a lap-old record whose sequence number gained a high bit compares
// newer than everything. The position must still follow the record
// restored: taken from that header instead, it would sit in the old block,
// and the step over the debris there would take the block of the record
// restored — and erase it.
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
  memory.bytes()[2 * flash_section.slot_capacity + 9] |= std::byte{0x80};

  flash_store store{memory};
  image<schema> restored;
  auto const result = store.load(restored);
  if (!result.record.valid || result.slot != 0) return false;
  if (restored.get<"motor.p">() != 5) return false;
  if (store.next_slot() != 1) return false;

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

  return true;
}

// A restart is not the only way the chain breaks. An erase that is refused
// leaves its block uncleared with nothing written, while the newest record
// lives in the other block — so the slot is not spent, and the next save
// erases the same block again rather than moving on to one it must keep.
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
  if (store.next_slot() != 0) return false;

  // The sixth erases the block and takes slot 0. The record in slot 3
  // stands throughout.
  if (!values.set<"motor.p">(std::int32_t{6})) return false;
  if (!store.save(values)) return false;
  if (store.next_slot() != 1) return false;
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
    if (store.save(values)) return false;                    // erase refused

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
inline constexpr section wide_section{.magic = magic,
                                      .base = 0,
                                      .slot_capacity = 64,
                                      .slot_count = 64,
                                      .slots_per_block = 16};
using wide_store = store<schema, wide, wide_section>;

consteval bool test_more_slots_than_a_word_has_bits()
{
  wide memory;
  wide_store store{memory};
  image<schema> values;

  // Past one full lap, so the newest record sits in a slot the search must
  // reach after wrapping.
  for (auto i = 1uz; i <= 70; ++i) {
    if (!values.set<"motor.p">(static_cast<std::int32_t>((i % 60) + 1))) {
      return false;
    }
    if (!store.save(values)) return false;
  }

  wide_store restarted{memory};
  image<schema> restored;
  auto const result = restarted.load(restored);

  if (!result.record.valid) return false;
  if (result.record.seq != 70) return false;
  if (result.slot != (70 - 1) % 64) return false;
  if (restored.get<"motor.p">() != static_cast<std::int32_t>((70 % 60) + 1)) {
    return false;
  }

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

consteval bool test_saving_before_loading_keeps_the_sequence()
{
  fram memory;
  image<schema> values;

  {
    fram_store store{memory};
    if (!store.save(values)) return false;   // slot 0, seq 1
    if (!store.save(values)) return false;   // slot 1, seq 2
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

    store<next_schema, fram, fram_section> newer{memory};
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
static_assert(test_a_write_that_does_not_stick_is_caught());
static_assert(test_a_save_that_landed_but_could_not_be_read_back());
static_assert(test_flash_rolls_over_between_blocks());
static_assert(test_flash_erase_never_takes_the_last_good_record());
static_assert(test_a_restart_onto_the_debris_of_a_torn_save());
static_assert(test_a_restart_onto_debris_with_no_header());
static_assert(test_a_lap_old_header_that_rotted_newer());
static_assert(test_a_save_after_an_erase_that_failed());
static_assert(test_a_torn_save_after_an_erase_that_failed());
static_assert(test_flash_round_trip());
static_assert(test_more_slots_than_a_word_has_bits());
static_assert(test_wipe());
static_assert(test_saving_before_loading_keeps_the_sequence());
static_assert(test_a_record_written_by_a_richer_firmware_still_loads());

} // namespace
