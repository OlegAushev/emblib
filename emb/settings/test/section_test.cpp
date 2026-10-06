#include <cstdint>

#include <emb/settings/section.hpp>
#include <emb/test/mock/ram_storage.hpp>

namespace {

using namespace emb;
using namespace emb::settings;

enum class group : std::uint8_t {
  drive,
  motor,
  far = 200
};

// Group drive declares its more demanding value after the less demanding
// one and group motor before it, and a writable value follows the read-only
// one, so that the order of the cells hides nothing a loop gets wrong.
inline constexpr auto schema = make_schema(
    param("drive.runout",
          std::uint32_t{100},
          {.max = std::uint32_t{1000},
           .group = group::drive,
           .apply = apply_policy::live}),
    param("drive.phase_swap",
          false,
          {.group = group::drive, .apply = apply_policy::on_safe_state}),
    param("motor.p",
          std::int32_t{11},
          {.min = std::int32_t{1},
           .max = std::int32_t{64},
           .group = group::motor,
           .apply = apply_policy::on_restart}),
    param("prod.serial",
          std::uint32_t{0},
          {.group = group::motor, .writable = false}),
    param("motor.R",
          0.0014f,
          {.min = 0.0f,
           .max = 10.0f,
           .group = group::motor,
           .apply = apply_policy::on_safe_state}),
    param("far.k",
          std::uint32_t{1},
          {.group = group::far, .apply = apply_policy::live}));

using fram = test::ram_storage<512>;
inline constexpr placement fram_placement{.magic = 0x47464354u,
                                          .base = 0,
                                          .slot_capacity = 128,
                                          .slot_count = 2};

using test_section = section<schema, fram, fram_placement>;

constexpr group_id drive{group::drive};
constexpr group_id motor{group::motor};
constexpr group_id far{group::far};

constexpr auto runout = parameter<schema, "drive.runout">::index;
constexpr auto pole_pairs = parameter<schema, "motor.p">::index;
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
  if (settings.unapplied()) return false;

  return true;
}

consteval bool test_a_write_is_unapplied_until_taken()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);

  if (!settings.set<"drive.runout">(std::uint32_t{200})) return false;
  if (settings.get<"drive.runout">() != 200) return false;
  if (!settings.unapplied()) return false;
  if (settings.restart_required()) return false;
  if (!settings.unsaved()) return false;
  if (settings.take(motor, apply_policy::on_restart)) return false;

  // Taken once; applied is not saved.
  if (!settings.take(drive, apply_policy::live)) return false;
  if (settings.take(drive, apply_policy::live)) return false;
  if (settings.unapplied()) return false;
  if (!settings.unsaved()) return false;

  if (!settings.set<"drive.runout">(std::uint32_t{200})) return false;
  if (settings.unapplied()) return false;

  if (settings.set<"drive.runout">(std::uint32_t{1001})) return false;
  if (settings.unapplied()) return false;

  return true;
}

// A group is applied whole: a take that cannot honour one of its values
// takes none of them, and one that can takes them all.
consteval bool test_a_group_is_taken_whole()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);

  if (!settings.set<"drive.phase_swap">(true)) return false;
  if (!settings.set<"drive.runout">(std::uint32_t{200})) return false;
  if (settings.take(drive, apply_policy::live)) return false;
  if (!settings.unapplied(apply_policy::live)) return false;
  if (!settings.unapplied(apply_policy::on_safe_state)) return false;

  if (!settings.take(drive, apply_policy::on_safe_state)) return false;
  if (settings.unapplied()) return false;

  // A value that needs a restart refuses its group to everyone else.
  if (!settings.set<"motor.p">(std::int32_t{7})) return false;
  if (!settings.set<"motor.R">(0.5f)) return false;
  if (settings.take(motor, apply_policy::on_safe_state)) return false;
  if (!settings.unapplied(apply_policy::on_safe_state)) return false;

  if (!settings.take(motor, apply_policy::on_restart)) return false;
  if (settings.restart_required()) return false;
  if (settings.unapplied()) return false;

  return true;
}

consteval bool test_groups_are_independent()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);

  if (!settings.set<"drive.runout">(std::uint32_t{200})) return false;
  if (!settings.set<"motor.R">(0.5f)) return false;

  if (!settings.take(drive, apply_policy::live)) return false;
  if (!settings.unapplied(apply_policy::on_safe_state)) return false;
  if (!settings.take(motor, apply_policy::on_safe_state)) return false;
  if (settings.unapplied()) return false;

  return true;
}

// A group is not a bit of a mask: any one-byte identifier works, and none
// stands for another.
consteval bool test_any_group_id_works()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);

  if (!settings.set<"far.k">(std::uint32_t{2})) return false;
  if (!settings.unapplied()) return false;
  if (!settings.unapplied(apply_policy::live)) return false;
  if (!settings.unsaved()) return false;
  if (settings.take(group_id{200 % 32}, apply_policy::live)) return false;
  if (!settings.take(far, apply_policy::live)) return false;
  if (settings.unapplied()) return false;

  return true;
}

// `unapplied(p)` answers for the policy `p` alone, and `restart_required()`
// for `on_restart`.
consteval bool test_unapplied_by_policy()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  auto const owes = [&settings](bool live, bool safe, bool restart) {
    return (settings.unapplied(apply_policy::live) == live)
        && (settings.unapplied(apply_policy::on_safe_state) == safe)
        && (settings.unapplied(apply_policy::on_restart) == restart)
        && (settings.restart_required() == restart);
  };

  if (!owes(false, false, false)) return false;

  if (!settings.set<"drive.runout">(std::uint32_t{200})) return false;
  if (!owes(true, false, false)) return false;
  if (!settings.take(drive, apply_policy::live)) return false;

  if (!settings.set<"motor.R">(0.5f)) return false;
  if (!owes(false, true, false)) return false;
  if (!settings.take(motor, apply_policy::on_safe_state)) return false;

  if (!settings.set<"motor.p">(std::int32_t{7})) return false;
  if (!owes(false, false, true)) return false;
  if (!settings.take(motor, apply_policy::on_restart)) return false;

  if (!owes(false, false, false)) return false;

  return true;
}

// Writing what a cell already holds owes nothing, not even a restart,
// whichever path the write takes.
consteval bool test_an_unchanged_write_owes_nothing()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  auto const owes_nothing = [&settings] {
    return !settings.unapplied() && !settings.restart_required();
  };

  if (!settings.set<"motor.p">(std::int32_t{11})) return false;
  if (!owes_nothing()) return false;
  if (!settings.set_at(pole_pairs, value{std::int32_t{11}})) return false;
  if (!owes_nothing()) return false;
  if (!settings.restore_default_at(pole_pairs)) return false;
  if (!owes_nothing()) return false;
  settings.restore_all_defaults();
  if (!owes_nothing()) return false;

  return true;
}

// A value put back owes nothing, whether written back or restored: no
// restart, nothing to apply or to save, and its group is free again.
consteval bool test_a_value_put_back_owes_nothing()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);

  if (!settings.set<"motor.p">(std::int32_t{7})) return false;
  if (!settings.restart_required()) return false;
  if (!settings.unapplied()) return false;
  if (!settings.unsaved()) return false;
  if (settings.take(motor, apply_policy::on_safe_state)) return false;

  if (!settings.set<"motor.p">(std::int32_t{11})) return false;
  if (settings.restart_required()) return false;
  if (settings.unapplied()) return false;
  if (settings.unsaved()) return false;
  if (settings.take(motor, apply_policy::on_restart)) return false;

  if (!settings.set<"motor.p">(std::int32_t{7})) return false;
  if (!settings.restart_required()) return false;
  if (!settings.restore_default_at(pole_pairs)) return false;
  if (settings.restart_required()) return false;
  if (settings.unapplied()) return false;
  if (settings.unsaved()) return false;
  if (settings.take(motor, apply_policy::on_restart)) return false;

  if (!settings.set<"motor.R">(0.5f)) return false;
  if (!settings.take(motor, apply_policy::on_safe_state)) return false;

  return true;
}

// A change that waits for a safe state, once put back, no longer holds back
// the live changes of its group.
consteval bool test_a_change_put_back_releases_its_group()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);

  if (!settings.set<"drive.phase_swap">(true)) return false;
  if (!settings.set<"drive.runout">(std::uint32_t{200})) return false;
  if (settings.take(drive, apply_policy::live)) return false;
  if (!settings.unapplied(apply_policy::live)) return false;

  if (!settings.set<"drive.phase_swap">(false)) return false;
  if (!settings.take(drive, apply_policy::live)) return false;
  if (settings.get<"drive.phase_swap">()) return false;
  if (settings.unapplied()) return false;
  if (settings.take(drive, apply_policy::live)) return false;

  return true;
}

// An adopted value counts as applied: it is saved like any other and calls
// for no take, and the value it replaced is a change from it.
consteval bool test_adopt_records_as_applied()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);

  if (!settings.adopt<"motor.R">(0.5f)) return false;
  if (settings.get<"motor.R">() != 0.5f) return false;
  if (settings.unapplied()) return false;
  if (!settings.unsaved()) return false;
  if (settings.take(motor, apply_policy::on_safe_state)) return false;

  if (!settings.set<"motor.R">(0.0014f)) return false;
  if (!settings.unapplied(apply_policy::on_safe_state)) return false;
  if (!settings.take(motor, apply_policy::on_safe_state)) return false;

  // Refused, it changes neither the value nor what was applied.
  if (!settings.set<"motor.R">(0.25f)) return false;
  auto const refused = settings.adopt<"motor.R">(11.0f);
  if (refused || refused.error() != error::out_of_range) return false;
  if (settings.get<"motor.R">() != 0.25f) return false;
  if (!settings.unapplied(apply_policy::on_safe_state)) return false;
  if (!settings.set<"motor.R">(0.0014f)) return false;
  if (settings.unapplied()) return false;

  // It records its own parameter, not the rest of the group.
  if (!settings.set<"motor.p">(std::int32_t{7})) return false;
  if (!settings.adopt<"motor.R">(0.5f)) return false;
  if (settings.unapplied(apply_policy::on_safe_state)) return false;
  if (!settings.restart_required()) return false;

  // Like `set`, it writes what a protocol may not.
  if (!settings.adopt<"prod.serial">(std::uint32_t{7})) return false;
  if (settings.get<"prod.serial">() != 7) return false;

  return true;
}

consteval bool test_save_and_restart()
{
  fram memory;
  {
    test_section settings;
    auto _ = settings.load(memory);
    if (!settings.set<"motor.p">(std::int32_t{7})) return false;

    // Saved is not applied: the restart is still owed.
    if (!settings.save()) return false;
    if (settings.unsaved()) return false;
    if (settings.sequence() != 1) return false;
    if (!settings.restart_required()) return false;

    if (!settings.set<"motor.p">(std::int32_t{8})) return false;
    if (!settings.unsaved()) return false;
    if (!settings.set<"motor.p">(std::int32_t{7})) return false;
    if (settings.unsaved()) return false;

    // Back to the value applied, it owes no restart, but the medium holds 7.
    if (!settings.set<"motor.p">(std::int32_t{11})) return false;
    if (settings.restart_required()) return false;
    if (settings.unapplied()) return false;
    if (!settings.unsaved()) return false;
  }

  test_section restarted;
  auto const result = restarted.load(memory);

  if (!result.record.valid) return false;
  if (restarted.last_load().record.seq != 1) return false;
  if (restarted.get<"motor.p">() != 7) return false;
  if (restarted.sequence() != 1) return false;
  if (restarted.unsaved()) return false;
  if (restarted.unapplied()) return false;

  return true;
}

// A load records what it restored as applied: a value of the medium owes
// nothing, and the default is a change from it, written or restored.
consteval bool test_a_load_takes_what_it_restored_as_applied()
{
  fram memory;
  {
    test_section settings;
    auto _ = settings.load(memory);
    if (!settings.set<"motor.p">(std::int32_t{7})) return false;
    if (!settings.save()) return false;
  }

  test_section restarted;
  auto _ = restarted.load(memory);
  if (restarted.get<"motor.p">() != 7) return false;
  if (restarted.unapplied()) return false;

  if (!restarted.set<"motor.p">(std::int32_t{11})) return false;
  if (!restarted.restart_required()) return false;
  if (!restarted.set<"motor.p">(std::int32_t{7})) return false;
  if (restarted.restart_required()) return false;
  if (!restarted.restore_default_at(pole_pairs)) return false;
  if (!restarted.restart_required()) return false;

  return true;
}

consteval bool test_wipe_leaves_the_image()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  if (!settings.set<"drive.runout">(std::uint32_t{300})) return false;
  if (!settings.save()) return false;
  if (!settings.unapplied()) return false;

  // A wipe the medium refuses changes nothing the section reports.
  memory.set_power_budget(0);
  if (settings.wipe()) return false;
  if (settings.unsaved()) return false;
  if (settings.sequence() != 1) return false;
  memory.set_power_budget(fram::unlimited);

  if (!settings.wipe()) return false;
  if (settings.get<"drive.runout">() != 300) return false;
  if (settings.sequence() != 0) return false;
  if (!settings.unsaved()) return false;
  // What the application is owed stays owed, and what it took stays taken.
  if (!settings.unapplied()) return false;
  if (!settings.take(drive, apply_policy::live)) return false;
  if (!settings.wipe()) return false;
  if (settings.unapplied()) return false;

  if (!settings.restore_default_at(runout)) return false;
  if (settings.unsaved()) return false;

  test_section restarted;
  if (restarted.load(memory).record.valid) return false;
  if (restarted.get<"drive.runout">() != 100) return false;

  // A load records what it restored as applied, the defaults included.
  if (!settings.unapplied()) return false;
  if (settings.load(memory).record.valid) return false;
  if (settings.unapplied()) return false;

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

  if (!restarted.set<"drive.runout">(std::uint32_t{300})) return false;
  memory.set_power_budget(0);
  if (restarted.save()) return false;
  if (restarted.sequence() != 3) return false;
  if (!restarted.unsaved()) return false;

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
  if (settings.unapplied()) return false;

  if (!settings.set_at(runout, value{std::uint32_t{400}})) return false;
  if (!settings.take(drive, apply_policy::live)) return false;

  if (!settings.restore_default_at(runout)) return false;
  if (!settings.take(drive, apply_policy::live)) return false;
  if (!settings.restore_default_at(runout)) return false;
  if (settings.unapplied()) return false;

  auto const not_restored = settings.restore_default_at(serial);
  if (not_restored || not_restored.error() != error::read_only) return false;

  return true;
}

consteval bool test_restore_all_defaults_spares_the_read_only()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);

  // prod.serial chose no policy, so it waits for a restart.
  if (!settings.set<"prod.serial">(std::uint32_t{7})) return false;
  if (!settings.restart_required()) return false;
  if (!settings.set<"drive.runout">(std::uint32_t{300})) return false;
  if (!settings.set<"motor.R">(0.5f)) return false;
  if (!settings.set<"far.k">(std::uint32_t{2})) return false;
  if (!settings.take(drive, apply_policy::live)) return false;
  if (!settings.take(motor, apply_policy::on_restart)) return false;

  settings.restore_all_defaults();

  if (settings.get<"drive.runout">() != 100) return false;
  if (settings.get<"prod.serial">() != 7) return false;
  if (settings.get<"motor.R">() != 0.0014f) return false;
  if (settings.get<"far.k">() != 1) return false;
  if (!settings.take(drive, apply_policy::live)) return false;
  if (settings.restart_required()) return false;

  return true;
}

static_assert(test_load_from_an_empty_medium());
static_assert(test_a_write_is_unapplied_until_taken());
static_assert(test_a_group_is_taken_whole());
static_assert(test_groups_are_independent());
static_assert(test_any_group_id_works());
static_assert(test_unapplied_by_policy());
static_assert(test_an_unchanged_write_owes_nothing());
static_assert(test_a_value_put_back_owes_nothing());
static_assert(test_a_change_put_back_releases_its_group());
static_assert(test_adopt_records_as_applied());
static_assert(test_save_and_restart());
static_assert(test_a_load_takes_what_it_restored_as_applied());
static_assert(test_wipe_leaves_the_image());
static_assert(test_sequence_follows_the_medium());
static_assert(test_access_by_index());
static_assert(test_restore_all_defaults_spares_the_read_only());

} // namespace
