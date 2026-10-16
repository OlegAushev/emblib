#include <cstdint>
#include <optional>
#include <string>

#include <emb/settings/section.hpp>
#include <emb/test/mock/ram_storage.hpp>

namespace {

using namespace emb;
using namespace emb::settings;

enum class group : std::uint8_t {
  drive,
  motor,
  boot,
  far = 200
};

inline constexpr auto drive_runout =
    param("drive.runout",
          std::uint32_t{100},
          {.max = std::uint32_t{1000},
           .group = group::drive,
           .apply = apply_policy::on_command});
inline constexpr auto drive_phase_swap =
    param("drive.phase_swap",
          false,
          {.group = group::drive, .apply = apply_policy::on_command});
inline constexpr auto motor_p = param("motor.p",
                                      std::int32_t{11},
                                      {.min = std::int32_t{1},
                                       .max = std::int32_t{64},
                                       .group = group::motor,
                                       .apply = apply_policy::on_restart});
inline constexpr auto prod_serial =
    param("prod.serial",
          std::uint32_t{0},
          {.group = group::motor, .writable = false});
inline constexpr auto motor_R =
    param("motor.R",
          0.0014f,
          {.min = 0.0f,
           .max = 10.0f,
           .group = group::motor,
           .apply = apply_policy::on_command});
inline constexpr auto far_k =
    param("far.k",
          std::uint32_t{1},
          {.group = group::far, .apply = apply_policy::on_command});
// A group with nothing applied on command.
inline constexpr auto boot_mode =
    param("boot.mode", std::uint32_t{0}, {.group = group::boot});

// Group motor declares a value applied on command after two applied after a
// restart, one of them read-only, so that the order of the cells hides
// nothing a loop gets wrong.
inline constexpr auto schema = make_schema(drive_runout,
                                           drive_phase_swap,
                                           motor_p,
                                           prod_serial,
                                           motor_R,
                                           far_k,
                                           boot_mode);

using fram = test::ram_storage<512>;
inline constexpr placement fram_placement{.magic = 0x47464354u,
                                          .base = 0,
                                          .slot_capacity = 128,
                                          .slot_count = 2};

using test_section = section<schema, fram, fram_placement>;

constexpr group_id drive{group::drive};
constexpr group_id motor{group::motor};
constexpr group_id boot{group::boot};
constexpr group_id far{group::far};

constexpr auto runout = parameter<schema, drive_runout>::index;
constexpr auto pole_pairs = parameter<schema, motor_p>::index;
constexpr auto serial = parameter<schema, prod_serial>::index;

// An applier that writes down what a section asks of it: "c0 l a0 f0 u "
// for a check, the lock, an admission and a configure of group 0, and the
// end of the lock. It refuses the check of the group `invalid` and the
// admission of the group `busy`, and keeps what it configured drive.runout
// to.
struct journal {
  std::string calls;
  std::optional<std::uint8_t> invalid;
  std::optional<std::uint8_t> busy;
  std::uint32_t runout = 0;

  struct guard {
    journal* owner;

    constexpr ~guard()
    {
      owner->calls += "u ";
    }
  };

  constexpr void note(char what, group_id group)
  {
    calls += what;
    if (group.value >= 100) {
      calls += static_cast<char>('0' + group.value / 100);
    }
    if (group.value >= 10) {
      calls += static_cast<char>('0' + group.value / 10 % 10);
    }
    calls += static_cast<char>('0' + group.value % 10);
    calls += ' ';
  }

  constexpr bool check(group_id group, image<schema> const&)
  {
    note('c', group);
    return invalid != group.value;
  }

  constexpr guard lock()
  {
    calls += "l ";
    return guard{this};
  }

  constexpr bool admit(group_id group)
  {
    note('a', group);
    return busy != group.value;
  }

  constexpr void configure(group_id group, image<schema> const& applied)
  {
    note('f', group);
    if (group == drive) {
      runout = applied.get<drive_runout>();
    }
  }
};

// A checker that accepts all values but those of `refused`.
struct refusing {
  std::optional<group_id> refused;

  constexpr bool check(group_id group, image<schema> const&)
  {
    return refused != group;
  }
};

static_assert(some_applier<journal, schema>);
static_assert(some_checker<refusing, schema>);
static_assert(!some_applier<refusing, schema>);

consteval bool test_load_from_an_empty_medium()
{
  fram memory;
  test_section settings;

  auto const result = settings.load(memory);

  if (result.record.valid) return false;
  if (settings.last_load().record.valid) return false;
  if (settings.get<motor_p>() != 11) return false;
  if (settings.sequence() != 0) return false;
  if (settings.unsaved()) return false;
  if (settings.unapplied()) return false;
  if (settings.restart_required()) return false;
  if (settings.last_failure()) return false;

  return true;
}

// A write waits until it is applied. Applied, it takes effect through the
// applier, which checks its group, then admits and configures it under the
// lock; with nothing left to apply, the applier is not called.
consteval bool test_a_write_waits_until_applied()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  journal applier;

  if (!settings.set<drive_runout>(std::uint32_t{200})) return false;
  if (!settings.unapplied()) return false;
  if (!settings.unapplied(drive)) return false;
  if (settings.unapplied(motor)) return false;
  if (settings.restart_required()) return false;
  if (settings.applied().get<drive_runout>() != 100) return false;

  if (!settings.apply(runout, applier)) return false;
  if (applier.calls != "c0 l a0 f0 u ") return false;
  if (applier.runout != 200) return false;
  if (settings.applied().get<drive_runout>() != 200) return false;
  if (settings.unapplied()) return false;
  if (!settings.unsaved()) return false;

  applier.calls.clear();
  if (!settings.apply(runout, applier)) return false;
  if (!settings.apply_group(drive, applier)) return false;
  if (!settings.apply_all(applier)) return false;
  if (!applier.calls.empty()) return false;

  // A write that puts the value applied back owes nothing.
  if (!settings.set<drive_runout>(std::uint32_t{300})) return false;
  if (!settings.set<drive_runout>(std::uint32_t{200})) return false;
  if (settings.unapplied()) return false;

  return true;
}

// Applying is all or nothing: every group of a change is checked before the
// lock, and admitted under it, before any value is recorded as applied. A
// group that refuses leaves every value where it was, and is kept as the
// last failure, which a success does not clear.
consteval bool test_applying_is_all_or_nothing()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);

  if (!settings.set<drive_runout>(std::uint32_t{200})) return false;
  if (!settings.set<motor_R>(0.5f)) return false;
  if (!settings.set<far_k>(std::uint32_t{2})) return false;

  journal invalid;
  invalid.invalid = 1;
  if (settings.apply_all(invalid)
      != std::unexpected(apply_failure{apply_refusal::invalid, motor})) {
    return false;
  }
  if (invalid.calls != "c0 c1 ") return false;
  if (settings.last_failure() != apply_failure{apply_refusal::invalid, motor}) {
    return false;
  }
  if (settings.applied().get<drive_runout>() != 100) return false;

  journal busy;
  busy.busy = 200;
  if (settings.apply_all(busy)
      != std::unexpected(apply_failure{apply_refusal::not_admitted, far})) {
    return false;
  }
  if (busy.calls != "c0 c1 c200 l a0 a1 a200 u ") return false;
  if (settings.applied().get<drive_runout>() != 100) return false;
  if (!settings.unapplied(far)) return false;

  journal willing;
  if (!settings.apply_all(willing)) return false;
  if (willing.calls != "c0 c1 c200 l a0 a1 a200 f0 f1 f200 u ") return false;
  if (willing.runout != 200) return false;
  if (settings.applied().get<motor_R>() != 0.5f) return false;
  if (settings.unapplied()) return false;

  return settings.last_failure()
      == apply_failure{apply_refusal::not_admitted, far};
}

// A group applies its own values: those of other groups keep waiting, and
// the check sees the values applied of the rest.
consteval bool test_a_group_applies_its_own()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  journal applier;

  if (!settings.set<drive_runout>(std::uint32_t{200})) return false;
  if (!settings.set<drive_phase_swap>(true)) return false;
  if (!settings.set<motor_R>(0.5f)) return false;

  if (!settings.apply_group(drive, applier)) return false;
  if (applier.calls != "c0 l a0 f0 u ") return false;
  if (!settings.applied().get<drive_phase_swap>()) return false;
  if (settings.unapplied(drive)) return false;
  if (!settings.unapplied(motor)) return false;

  // A parameter applies itself alone.
  if (!settings.set<drive_runout>(std::uint32_t{300})) return false;
  if (!settings.set<drive_phase_swap>(false)) return false;
  if (!settings.apply(runout, applier)) return false;
  if (!settings.applied().get<drive_phase_swap>()) return false;
  if (!settings.unapplied(drive)) return false;

  return true;
}

// The schema decides first: a parameter applied after a restart, or a
// group with nothing applied on command, is refused whatever its values. A
// group whose values applied on command are all applied succeeds without
// the applier, and a value waiting for a restart keeps waiting.
consteval bool test_the_schema_refuses_first()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  journal applier;

  if (settings.apply(pole_pairs, applier)
      != std::unexpected(apply_failure{apply_refusal::not_applicable, motor})) {
    return false;
  }
  if (settings.apply_group(boot, applier)
      != std::unexpected(apply_failure{apply_refusal::not_applicable, boot})) {
    return false;
  }
  if (settings.apply_group(group_id{7}, applier)) return false;
  if (settings.apply(image<schema>::count, applier)
      != std::unexpected(
          apply_failure{apply_refusal::unknown_parameter, group_id{}})) {
    return false;
  }

  if (!settings.set<motor_p>(std::int32_t{7})) return false;
  if (!settings.restart_required()) return false;
  if (settings.unapplied()) return false;
  if (settings.unapplied(motor)) return false;
  if (!settings.apply_group(motor, applier)) return false;
  if (!settings.apply_all(applier)) return false;
  if (!applier.calls.empty()) return false;
  if (settings.applied().get<motor_p>() != 11) return false;
  if (!settings.restart_required()) return false;

  return true;
}

// A group is not a bit of a mask: any one-byte identifier works, and none
// stands for another.
consteval bool test_any_group_id_works()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  journal applier;

  if (!settings.set<far_k>(std::uint32_t{2})) return false;
  if (!settings.unapplied(far)) return false;
  if (settings.unapplied(group_id{200 % 32})) return false;
  if (settings.apply_group(group_id{200 % 32}, applier)) return false;
  if (!settings.unapplied()) return false;
  if (!settings.apply_group(far, applier)) return false;
  if (applier.calls != "c200 l a200 f200 u ") return false;
  if (settings.unapplied()) return false;

  return true;
}

// Discarding puts the values applied back, whatever their policy, but not
// into a parameter a protocol may not write.
consteval bool test_discard_puts_back_what_is_applied()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);

  if (!settings.set<drive_runout>(std::uint32_t{200})) return false;
  if (!settings.set<motor_p>(std::int32_t{7})) return false;
  if (!settings.set<motor_R>(0.5f)) return false;
  if (!settings.set<prod_serial>(std::uint32_t{5})) return false;
  if (!settings.set<far_k>(std::uint32_t{2})) return false;

  if (!settings.discard(runout)) return false;
  if (settings.get<drive_runout>() != 100) return false;
  auto const read_only = settings.discard(serial);
  if (read_only || (read_only.error() != error::read_only)) return false;
  auto const unknown = settings.discard(image<schema>::count);
  if (unknown || (unknown.error() != error::unknown_parameter)) return false;

  settings.discard_group(motor);
  if (settings.get<motor_p>() != 11) return false;
  if (settings.get<motor_R>() != 0.0014f) return false;
  if (settings.get<prod_serial>() != 5) return false;
  if (!settings.unapplied(far)) return false;

  settings.discard_all();
  if (settings.get<far_k>() != 1) return false;
  if (settings.get<prod_serial>() != 5) return false;
  if (settings.unapplied()) return false;

  return true;
}

// `unapplied` answers for the values applied on command, and
// `restart_required` for those applied after a restart.
consteval bool test_unapplied_and_restart_required_split()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  journal applier;
  auto const owes = [&settings](bool command, bool restart) {
    return (settings.unapplied() == command)
        && (settings.restart_required() == restart);
  };

  if (!owes(false, false)) return false;
  if (!settings.set<motor_R>(0.5f)) return false;
  if (!owes(true, false)) return false;
  if (!settings.set<motor_p>(std::int32_t{7})) return false;
  if (!owes(true, true)) return false;
  if (!settings.apply_all(applier)) return false;
  if (!owes(false, true)) return false;
  if (!settings.set<motor_p>(std::int32_t{11})) return false;
  if (!owes(false, false)) return false;

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
    return !settings.unapplied()
        && !settings.restart_required()
        && !settings.unsaved();
  };

  if (!settings.set<motor_p>(std::int32_t{11})) return false;
  if (!owes_nothing()) return false;
  if (!settings.set_at(pole_pairs, value{std::int32_t{11}})) return false;
  if (!owes_nothing()) return false;
  settings.restore_all_defaults();
  if (!owes_nothing()) return false;

  return true;
}

// A save keeps what the machine runs on: it waits until the values applied
// on command are applied, and checks the values that wait for a restart.
consteval bool test_a_save_keeps_what_runs()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  journal applier;
  refusing refuses_motor{motor};

  if (!settings.set<drive_runout>(std::uint32_t{200})) return false;
  auto const unapplied = settings.save(refuses_motor);
  if (unapplied || (unapplied.error().reason != save_refusal::unapplied)) {
    return false;
  }
  if (settings.sequence() != 0) return false;

  if (!settings.apply_all(applier)) return false;
  if (!settings.set<motor_p>(std::int32_t{7})) return false;
  auto const invalid = settings.save(refuses_motor);
  if (invalid
      || (invalid.error().reason != save_refusal::invalid)
      || (invalid.error().group != motor)) {
    return false;
  }
  if (settings.last_failure() != apply_failure{apply_refusal::invalid, motor}) {
    return false;
  }
  if (settings.sequence() != 0) return false;

  refusing accepts;
  if (!settings.save(accepts)) return false;
  if (settings.sequence() != 1) return false;
  if (settings.unsaved()) return false;

  // A medium that fails says how.
  memory.set_power_budget(0);
  auto const failed = settings.save(accepts);
  if (failed
      || (failed.error().reason != save_refusal::medium)
      || !failed.error().store) {
    return false;
  }
  memory.set_power_budget(fram::unlimited);

  return true;
}

// A value saved and then put back still owes the restart that would load
// it, until it is saved again.
consteval bool test_a_saved_restart_stays_owed()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  refusing accepts;

  if (!settings.set<motor_p>(std::int32_t{7})) return false;
  if (!settings.save(accepts)) return false;
  if (!settings.restart_required()) return false;

  if (!settings.discard(pole_pairs)) return false;
  if (settings.get<motor_p>() != 11) return false;
  if (!settings.restart_required()) return false;
  if (!settings.unsaved()) return false;

  if (!settings.save(accepts)) return false;
  if (settings.restart_required()) return false;

  return true;
}

consteval bool test_save_and_restart()
{
  fram memory;
  refusing accepts;
  {
    test_section settings;
    auto _ = settings.load(memory);
    if (!settings.set<motor_p>(std::int32_t{7})) return false;

    // Saved is not applied: the restart is still owed.
    if (!settings.save(accepts)) return false;
    if (settings.unsaved()) return false;
    if (settings.sequence() != 1) return false;
    if (!settings.restart_required()) return false;

    if (!settings.set<motor_p>(std::int32_t{8})) return false;
    if (!settings.unsaved()) return false;
    if (!settings.set<motor_p>(std::int32_t{7})) return false;
    if (settings.unsaved()) return false;
  }

  test_section restarted;
  auto const result = restarted.load(memory);

  if (!result.record.valid) return false;
  if (restarted.last_load().record.seq != 1) return false;
  if (restarted.get<motor_p>() != 7) return false;
  if (restarted.sequence() != 1) return false;
  if (restarted.unsaved()) return false;
  if (restarted.unapplied()) return false;
  if (restarted.restart_required()) return false;

  return true;
}

// A load records what it restored as applied: a value of the medium owes
// nothing, and the default is a change from it.
consteval bool test_a_load_takes_what_it_restored_as_applied()
{
  fram memory;
  refusing accepts;
  {
    test_section settings;
    auto _ = settings.load(memory);
    if (!settings.set<motor_p>(std::int32_t{7})) return false;
    if (!settings.save(accepts)) return false;
  }

  test_section restarted;
  auto _ = restarted.load(memory);
  if (restarted.get<motor_p>() != 7) return false;
  if (restarted.applied().get<motor_p>() != 7) return false;
  if (restarted.restart_required()) return false;

  if (!restarted.set<motor_p>(std::int32_t{11})) return false;
  if (!restarted.restart_required()) return false;
  if (!restarted.set<motor_p>(std::int32_t{7})) return false;
  if (restarted.restart_required()) return false;
  restarted.restore_all_defaults();
  if (!restarted.restart_required()) return false;

  return true;
}

// A group found invalid at startup is built from its defaults: they become
// its values applied, while the values loaded wait to be corrected or
// discarded, and a save waits with them.
consteval bool test_a_group_falls_back_to_its_defaults()
{
  fram memory;
  refusing accepts;
  {
    test_section settings;
    auto _ = settings.load(memory);
    journal applier;
    if (!settings.set<drive_runout>(std::uint32_t{300})) return false;
    if (!settings.apply_all(applier)) return false;
    if (!settings.save(accepts)) return false;
  }

  test_section restarted;
  auto _ = restarted.load(memory);
  restarted.fall_back_to_defaults(drive);
  if (restarted.applied().get<drive_runout>() != 100) return false;
  if (restarted.get<drive_runout>() != 300) return false;
  if (restarted.stored().get<drive_runout>() != 300) return false;
  if (!restarted.unapplied(drive)) return false;
  if (restarted.unapplied(motor)) return false;
  if (restarted.save(accepts)) return false;

  restarted.discard_group(drive);
  if (restarted.get<drive_runout>() != 100) return false;
  if (!restarted.save(accepts)) return false;

  return true;
}

consteval bool test_wipe_leaves_the_image()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  journal applier;
  refusing accepts;
  if (!settings.set<drive_runout>(std::uint32_t{300})) return false;
  if (!settings.apply_all(applier)) return false;
  if (!settings.save(accepts)) return false;

  // A wipe the medium refuses changes nothing the section reports.
  memory.set_power_budget(0);
  if (settings.wipe()) return false;
  if (settings.unsaved()) return false;
  if (settings.sequence() != 1) return false;
  memory.set_power_budget(fram::unlimited);

  if (!settings.wipe()) return false;
  if (settings.get<drive_runout>() != 300) return false;
  if (settings.sequence() != 0) return false;
  if (!settings.unsaved()) return false;
  // What was applied stays applied.
  if (settings.applied().get<drive_runout>() != 300) return false;
  if (settings.unapplied()) return false;

  if (!settings.set<drive_runout>(std::uint32_t{100})) return false;
  if (settings.unsaved()) return false;

  test_section restarted;
  if (restarted.load(memory).record.valid) return false;
  if (restarted.get<drive_runout>() != 100) return false;

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
  refusing accepts;
  test_section settings;
  auto _ = settings.load(memory);
  if (settings.sequence() != 0) return false;

  for (auto seq = 1u; seq <= 3; ++seq) {
    if (!settings.save(accepts)) return false;
    if (settings.sequence() != seq) return false;
  }

  test_section restarted;
  auto _ = restarted.load(memory);
  if (restarted.last_load().record.seq != 3) return false;
  if (restarted.sequence() != 3) return false;

  journal applier;
  if (!restarted.set<drive_runout>(std::uint32_t{300})) return false;
  if (!restarted.apply_all(applier)) return false;
  memory.set_power_budget(0);
  if (restarted.save(accepts)) return false;
  if (restarted.sequence() != 3) return false;
  if (!restarted.unsaved()) return false;

  memory.set_power_budget(fram::unlimited);
  if (!restarted.save(accepts)) return false;
  if (restarted.sequence() != 4) return false;

  memory.set_read_fault(true);
  test_section blind;
  auto _ = blind.load(memory);
  if (!blind.last_load().read_failed) return false;
  if (blind.sequence() != 0) return false;

  memory.set_read_fault(false);
  if (!blind.save(accepts)) return false;
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
  if (!settings.unapplied()) return false;

  return true;
}

// The three images part and meet: a write changes the values requested, an
// application the values applied, a save the values stored.
consteval bool test_the_images_part_and_meet()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  journal applier;
  refusing accepts;

  if (!settings.set<drive_runout>(std::uint32_t{200})) return false;
  if (settings.values().get<drive_runout>() != 200) return false;
  if (settings.applied().get<drive_runout>() != 100) return false;
  if (settings.stored().get<drive_runout>() != 100) return false;
  if (settings.applied_at(runout) != value{std::uint32_t{100}}) return false;
  if (settings.stored_at(runout) != value{std::uint32_t{100}}) return false;

  if (!settings.apply(runout, applier)) return false;
  if (settings.applied_at(runout) != value{std::uint32_t{200}}) return false;
  if (settings.stored_at(runout) != value{std::uint32_t{100}}) return false;

  if (!settings.save(accepts)) return false;
  if (settings.stored().get<drive_runout>() != 200) return false;

  auto const unknown = settings.applied_at(image<schema>::count);
  if (unknown || unknown.error() != error::unknown_parameter) return false;
  auto const unstored = settings.stored_at(image<schema>::count);
  return !unstored && (unstored.error() == error::unknown_parameter);
}

consteval bool test_restore_all_defaults_spares_the_read_only()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  journal applier;

  // prod.serial chose no policy, so it waits for a restart.
  if (!settings.set<prod_serial>(std::uint32_t{7})) return false;
  if (!settings.restart_required()) return false;
  if (!settings.set<drive_runout>(std::uint32_t{300})) return false;
  if (!settings.set<motor_R>(0.5f)) return false;
  if (!settings.set<far_k>(std::uint32_t{2})) return false;
  if (!settings.apply_all(applier)) return false;

  settings.restore_all_defaults();

  if (settings.get<drive_runout>() != 100) return false;
  if (settings.get<prod_serial>() != 7) return false;
  if (settings.get<motor_R>() != 0.0014f) return false;
  if (settings.get<far_k>() != 1) return false;
  // The defaults are requested, not applied.
  if (!settings.unapplied()) return false;
  if (settings.applied().get<drive_runout>() != 300) return false;
  if (!settings.restart_required()) return false;

  return true;
}

static_assert(test_load_from_an_empty_medium());
static_assert(test_a_write_waits_until_applied());
static_assert(test_applying_is_all_or_nothing());
static_assert(test_a_group_applies_its_own());
static_assert(test_the_schema_refuses_first());
static_assert(test_any_group_id_works());
static_assert(test_discard_puts_back_what_is_applied());
static_assert(test_unapplied_and_restart_required_split());
static_assert(test_an_unchanged_write_owes_nothing());
static_assert(test_a_save_keeps_what_runs());
static_assert(test_a_saved_restart_stays_owed());
static_assert(test_save_and_restart());
static_assert(test_a_load_takes_what_it_restored_as_applied());
static_assert(test_a_group_falls_back_to_its_defaults());
static_assert(test_wipe_leaves_the_image());
static_assert(test_sequence_follows_the_medium());
static_assert(test_access_by_index());
static_assert(test_the_images_part_and_meet());
static_assert(test_restore_all_defaults_spares_the_read_only());

} // namespace
