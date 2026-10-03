#include <cstdint>

#include <emb/settings/section.hpp>
#include <emb/test/mock/block_storage.hpp>
#include <emb/test/mock/plain_word.hpp>

namespace {

using namespace emb;
using namespace emb::settings;

enum class group : std::uint8_t {
  drive,
  motor
};

inline constexpr auto schema = make_schema(
    param("drive.phase_swap",
          false,
          {.group = group::drive, .apply = apply_policy::on_safe_state}),
    param("drive.runout",
          std::uint32_t{100},
          {.max = std::uint32_t{1000},
           .group = group::drive,
           .apply = apply_policy::live}),
    param("motor.p",
          std::int32_t{11},
          {.min = std::int32_t{1},
           .max = std::int32_t{64},
           .group = group::motor,
           .apply = apply_policy::on_restart}),
    param("prod.serial",
          std::uint32_t{0},
          {.group = group::motor, .writable = false}));

using fram = test::block_storage<512>;
inline constexpr placement fram_placement{.magic = 0x47464354u,
                                          .base = 0,
                                          .slot_capacity = 128,
                                          .slot_count = 2};

using test_section = section<schema, fram, fram_placement, test::plain_word>;

constexpr group_id drive{group::drive};
constexpr group_id motor{group::motor};

constexpr auto runout = parameter<schema, "drive.runout">::index;
constexpr auto serial = parameter<schema, "prod.serial">::index;

consteval bool test_load_from_an_empty_medium()
{
  fram memory;
  test_section settings;

  auto const result = settings.load(memory);

  if (result.record.valid) return false;
  if (settings.last_load().record.valid) return false;
  if (settings.get<"motor.p">() != 11) return false;
  if (settings.sequence() != 0) return false;
  if (settings.unsaved()) return false;
  if (settings.pending().any()) return false;

  return true;
}

consteval bool test_a_write_marks_its_group()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);

  if (!settings.set<"drive.runout">(std::uint32_t{200})) return false;
  if (settings.get<"drive.runout">() != 200) return false;
  if (!settings.pending().changed(drive, apply_policy::live)) return false;
  if (settings.pending().changed(motor, apply_policy::on_restart)) {
    return false;
  }
  if (!settings.unsaved()) return false;

  if (!settings.pending().take(drive, apply_policy::live)) return false;
  if (!settings.set<"drive.runout">(std::uint32_t{200})) return false;
  if (settings.pending().any()) return false;

  if (settings.set<"drive.runout">(std::uint32_t{1001})) return false;
  if (settings.pending().any()) return false;

  return true;
}

consteval bool test_save_and_restart()
{
  fram memory;
  {
    test_section settings;
    auto _ = settings.load(memory);
    if (!settings.set<"motor.p">(std::int32_t{7})) return false;

    if (!settings.save()) return false;
    if (settings.unsaved()) return false;
    if (settings.sequence() != 1) return false;
    if (!settings.pending().restart_required()) return false;

    if (!settings.set<"motor.p">(std::int32_t{8})) return false;
    if (!settings.unsaved()) return false;
    if (!settings.set<"motor.p">(std::int32_t{7})) return false;
    if (settings.unsaved()) return false;
  }

  test_section restarted;
  auto const result = restarted.load(memory);

  if (!result.record.valid) return false;
  if (restarted.last_load().record.seq != 1) return false;
  if (restarted.get<"motor.p">() != 7) return false;
  if (restarted.sequence() != 1) return false;
  if (restarted.unsaved()) return false;
  if (restarted.pending().any()) return false;

  return true;
}

consteval bool test_wipe_leaves_the_image()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  if (!settings.set<"drive.runout">(std::uint32_t{300})) return false;
  if (!settings.save()) return false;

  if (!settings.wipe()) return false;
  if (settings.get<"drive.runout">() != 300) return false;
  if (settings.sequence() != 0) return false;
  if (!settings.unsaved()) return false;

  if (!settings.restore_default_at(runout)) return false;
  if (settings.unsaved()) return false;

  test_section restarted;
  if (restarted.load(memory).record.valid) return false;
  if (restarted.get<"drive.runout">() != 100) return false;

  return true;
}

// `sequence()` is the number of a record on the medium: the one a load
// restored or a save wrote, zero if there is none. A failed save wrote
// nothing and leaves it, and the save after a load that could not read
// numbers past the records on the medium, not past zero.
consteval bool test_sequence_follows_the_medium()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  if (settings.sequence() != 0) return false;

  for (auto seq = 1u; seq <= 3; ++seq) {
    if (!settings.save()) return false;
    if (settings.sequence() != seq) return false;
  }

  test_section restarted;
  auto _ = restarted.load(memory);
  if (restarted.last_load().record.seq != 3) return false;
  if (restarted.sequence() != 3) return false;

  memory.set_power_budget(0);
  if (restarted.save()) return false;
  if (restarted.sequence() != 3) return false;

  memory.set_power_budget(fram::unlimited);
  if (!restarted.save()) return false;
  if (restarted.sequence() != 4) return false;

  memory.set_read_fault(true);
  test_section blind;
  auto _ = blind.load(memory);
  if (!blind.last_load().read_failed) return false;
  if (blind.sequence() != 0) return false;

  memory.set_read_fault(false);
  if (!blind.save()) return false;
  if (blind.sequence() != 5) return false;

  test_section again;
  auto _ = again.load(memory);
  if (again.sequence() != blind.sequence()) return false;

  if (!again.wipe()) return false;
  if (again.sequence() != 0) return false;

  return true;
}

consteval bool test_access_by_index()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);

  if (settings.get_at(runout) != value{std::uint32_t{100}}) return false;

  auto const read_only = settings.set_at(serial, value{std::uint32_t{5}});
  if (read_only || read_only.error() != error::read_only) return false;
  auto const retyped = settings.set_at(runout, value{std::int32_t{5}});
  if (retyped || retyped.error() != error::type_mismatch) return false;
  auto const too_far = settings.set_at(runout, value{std::uint32_t{1001}});
  if (too_far || too_far.error() != error::out_of_range) return false;
  if (settings.pending().any()) return false;

  if (!settings.set_at(runout, value{std::uint32_t{400}})) return false;
  if (!settings.pending().take(drive, apply_policy::live)) return false;

  if (!settings.restore_default_at(runout)) return false;
  if (!settings.pending().take(drive, apply_policy::live)) return false;
  if (!settings.restore_default_at(runout)) return false;
  if (settings.pending().any()) return false;

  auto const not_restored = settings.restore_default_at(serial);
  if (not_restored || not_restored.error() != error::read_only) return false;

  return true;
}

consteval bool test_restore_all_defaults_spares_the_read_only()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);

  if (!settings.set<"prod.serial">(std::uint32_t{7})) return false;
  if (!settings.set<"drive.runout">(std::uint32_t{300})) return false;
  settings.pending().clear();

  settings.restore_all_defaults();

  if (settings.get<"drive.runout">() != 100) return false;
  if (settings.get<"prod.serial">() != 7) return false;
  if (!settings.pending().changed(drive, apply_policy::live)) return false;
  if (settings.pending().restart_required()) return false;

  return true;
}

// The production instantiation must compile for the target too, not only
// the test double it is checked through.
[[maybe_unused]] void instantiate_atomic_section()
{
  fram memory;
  section<schema, fram, fram_placement> settings;
  [[maybe_unused]] auto const loaded = settings.load(memory);
  [[maybe_unused]] auto const set =
      settings.set<"drive.runout">(std::uint32_t{1});
  [[maybe_unused]] auto const any = settings.pending().any();
  [[maybe_unused]] auto const saved = settings.save();
}

static_assert(test_load_from_an_empty_medium());
static_assert(test_a_write_marks_its_group());
static_assert(test_save_and_restart());
static_assert(test_wipe_leaves_the_image());
static_assert(test_sequence_follows_the_medium());
static_assert(test_access_by_index());
static_assert(test_restore_all_defaults_spares_the_read_only());

} // namespace
