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
static_assert(test_a_corrupted_record_falls_back_to_the_previous_one());
static_assert(test_a_write_that_does_not_stick_is_caught());
static_assert(test_a_save_that_landed_but_could_not_be_read_back());
static_assert(test_flash_rolls_over_between_blocks());
static_assert(test_flash_erase_never_takes_the_last_good_record());
static_assert(test_flash_round_trip());
static_assert(test_wipe());
static_assert(test_saving_before_loading_keeps_the_sequence());
static_assert(test_a_record_written_by_a_richer_firmware_still_loads());

} // namespace
