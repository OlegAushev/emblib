#include <array>
#include <cstdint>
#include <string>

#include <emb/meta/typelist.hpp>
#include <emb/settings/rule.hpp>
#include <emb/settings/section.hpp>
#include <emb/test/mock/ram_storage.hpp>

namespace {

using namespace emb;
using namespace emb::settings;

inline constexpr auto drive_runout = param("drive.runout",
                                           std::uint32_t{100},
                                           {.title = {"Runout", "Выбег"},
                                            .max = std::uint32_t{1000},
                                            .apply = apply_policy::on_command});
inline constexpr auto drive_phase_swap =
    param("drive.phase_swap",
          false,
          {.title = {"Phase swap", "Перестановка фаз"},
           .apply = apply_policy::on_command});
inline constexpr auto motor_p =
    param("motor.p",
          std::int32_t{11},
          {.title = {"Pole pairs", "Число пар полюсов"},
           .min = std::int32_t{1},
           .max = std::int32_t{64},
           .apply = apply_policy::on_restart});
inline constexpr auto prod_serial =
    param("prod.serial",
          std::uint32_t{0},
          {.title = {"Serial number", "Серийный номер"}, .writable = false});
inline constexpr auto motor_R =
    param("motor.R",
          0.0014f,
          {.title = {"Stator resistance", "Сопротивление статора"},
           .min = 0.0f,
           .max = 10.0f,
           .apply = apply_policy::on_command});
inline constexpr auto limit_low =
    param("limit.low",
          std::uint32_t{10},
          {.title = {"Low limit", "Нижний предел"},
           .apply = apply_policy::on_command});
inline constexpr auto limit_high =
    param("limit.high",
          std::uint32_t{20},
          {.title = {"High limit", "Верхний предел"},
           .apply = apply_policy::on_command});
inline constexpr auto limit_top =
    param("limit.top",
          std::uint32_t{30},
          {.title = {"Top limit", "Наибольший предел"},
           .apply = apply_policy::on_command});

// motor.R, applied on command, follows two parameters applied after a
// restart, one of them read-only, so that the order of the cells hides
// nothing a loop gets wrong.
inline constexpr auto schema = make_schema(drive_runout,
                                           drive_phase_swap,
                                           motor_p,
                                           prod_serial,
                                           motor_R,
                                           limit_low,
                                           limit_high,
                                           limit_top);

using fram = test::ram_storage<512>;
inline constexpr placement fram_placement{.magic = 0x47464354u,
                                          .base = 0,
                                          .slot_capacity = 128,
                                          .slot_count = 2};

using test_section = section<schema, fram, fram_placement>;

constexpr auto runout = parameter<schema, drive_runout>::index;
constexpr auto pole_pairs = parameter<schema, motor_p>::index;
constexpr auto serial = parameter<schema, prod_serial>::index;
constexpr auto lower = parameter<schema, limit_low>::index;
constexpr auto upper = parameter<schema, limit_high>::index;

// limit.low lies below limit.high.
struct ordered {
  using in = inputs<schema, limit_low, limit_high>;

  static constexpr bool holds(in v)
  {
    return v.get<limit_low>() < v.get<limit_high>();
  }
};

// limit.high does not exceed limit.top: it shares limit.high with `ordered`.
struct capped {
  using in = inputs<schema, limit_high, limit_top>;

  static constexpr bool holds(in v)
  {
    return v.get<limit_high>() <= v.get<limit_top>();
  }
};

// A rule over a parameter applied after a restart.
struct not_thirteen {
  using in = inputs<schema, motor_p>;

  static constexpr bool holds(in v)
  {
    return v.get<motor_p>() != 13;
  }
};

using rules = typelist<ordered, capped, not_thirteen>;

// An applier that writes down what a section asks of it: "c l a f u " for a
// check, the lock, an admission, a configure and the end of the lock. Its
// check holds the values to the rules and refuses them all while `invalid`,
// it admits nothing while `busy`, and it keeps what drive.runout was and
// became in the last change it configured.
struct journal {
  std::string calls;
  bool invalid = false;
  bool busy = false;
  std::uint32_t runout_before = 0;
  std::uint32_t runout_after = 0;

  struct guard {
    journal* owner;

    constexpr ~guard()
    {
      owner->calls += "u ";
    }
  };

  constexpr bool check(image<schema> const& values)
  {
    calls += "c ";
    return !invalid && all_hold(rules{}, values);
  }

  constexpr guard lock()
  {
    calls += "l ";
    return guard{this};
  }

  constexpr bool admit(image<schema> const&, image<schema> const&)
  {
    calls += "a ";
    return !busy;
  }

  constexpr void configure(image<schema> const& before,
                           image<schema> const& after)
  {
    calls += "f ";
    runout_before = before.get<drive_runout>();
    runout_after = after.get<drive_runout>();
  }
};

// An applier that takes any change, the rules notwithstanding.
struct anything {
  struct guard {};

  constexpr bool check(image<schema> const&)
  {
    return true;
  }

  constexpr guard lock()
  {
    return {};
  }

  constexpr bool admit(image<schema> const&, image<schema> const&)
  {
    return true;
  }

  constexpr void configure(image<schema> const&, image<schema> const&) {}
};

// A checker that accepts all values, or none if `refuses`.
struct refusing {
  bool refuses = false;

  constexpr bool check(image<schema> const&)
  {
    return !refuses;
  }
};

static_assert(some_applier<journal, schema>);
static_assert(some_applier<anything, schema>);
static_assert(some_checker<refusing, schema>);
static_assert(!some_applier<refusing, schema>);

// Writes `low`, `high` and `top` to the limits, applies them and saves them
// on `memory`, whatever the rules say.
consteval bool store_limits(fram& memory,
                            std::uint32_t low,
                            std::uint32_t high,
                            std::uint32_t top)
{
  test_section settings;
  auto _ = settings.load(memory);
  anything applier;
  refusing accepts;
  return settings.set<limit_low>(low)
      && settings.set<limit_high>(high)
      && settings.set<limit_top>(top)
      && settings.apply_all(applier)
      && settings.save(accepts);
}

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
  if (settings.last_refusal()) return false;

  return true;
}

// A write waits until it is applied. Applied, it takes effect through the
// applier, which checks the values it would bring about, then admits and
// configures the change under the lock, given the values applied before and
// after; with nothing left to apply, the applier is not called.
consteval bool test_a_write_waits_until_applied()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  journal applier;

  if (!settings.set<drive_runout>(std::uint32_t{200})) return false;
  if (!settings.unapplied()) return false;
  if (settings.restart_required()) return false;
  if (settings.applied().get<drive_runout>() != 100) return false;

  if (!settings.apply(runout, applier)) return false;
  if (applier.calls != "c l a f u ") return false;
  if ((applier.runout_before != 100) || (applier.runout_after != 200)) {
    return false;
  }
  if (settings.applied().get<drive_runout>() != 200) return false;
  if (settings.unapplied()) return false;
  if (!settings.unsaved()) return false;

  applier.calls.clear();
  if (!settings.apply(runout, applier)) return false;
  if (!settings.apply_all(applier)) return false;
  if (!applier.calls.empty()) return false;

  // A write that puts the value applied back owes nothing.
  if (!settings.set<drive_runout>(std::uint32_t{300})) return false;
  if (!settings.set<drive_runout>(std::uint32_t{200})) return false;
  if (settings.unapplied()) return false;

  return true;
}

// Applying is all or nothing: the values a change would bring about are
// checked before the lock, and the change is admitted under it, before any
// value is recorded as applied. A refusal leaves every value where it was
// and is kept as the last failure, which a success does not clear.
consteval bool test_applying_is_all_or_nothing()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);

  if (!settings.set<drive_runout>(std::uint32_t{200})) return false;
  if (!settings.set<motor_R>(0.5f)) return false;

  journal invalid;
  invalid.invalid = true;
  if (settings.apply_all(invalid) != std::unexpected(apply_refusal::invalid)) {
    return false;
  }
  if (invalid.calls != "c ") return false;
  if (settings.last_refusal() != refusal{apply_refusal::invalid}) return false;
  if (settings.applied().get<drive_runout>() != 100) return false;

  journal busy;
  busy.busy = true;
  if (settings.apply_all(busy)
      != std::unexpected(apply_refusal::not_admitted)) {
    return false;
  }
  if (busy.calls != "c l a u ") return false;
  if (settings.applied().get<drive_runout>() != 100) return false;
  if (!settings.unapplied()) return false;

  journal willing;
  if (!settings.apply_all(willing)) return false;
  if (willing.calls != "c l a f u ") return false;
  if (willing.runout_after != 200) return false;
  if (settings.applied().get<motor_R>() != 0.5f) return false;
  if (settings.unapplied()) return false;

  return settings.last_refusal() == refusal{apply_refusal::not_admitted};
}

// A parameter applies alone: the others keep waiting, and the check sees the
// values applied with this one taken in. A step that breaks a rule is
// refused and changes nothing; taken in the right order, both steps pass.
consteval bool test_a_parameter_applies_alone()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  journal applier;

  if (!settings.set<drive_runout>(std::uint32_t{200})) return false;
  if (!settings.set<drive_phase_swap>(true)) return false;
  if (!settings.apply(runout, applier)) return false;
  if (settings.applied().get<drive_phase_swap>()) return false;
  if (!settings.unapplied()) return false;

  // Both limits rise: limit.low first would pass limit.high.
  if (!settings.set<limit_low>(std::uint32_t{25})) return false;
  if (!settings.set<limit_high>(std::uint32_t{28})) return false;
  if (settings.apply(lower, applier)
      != std::unexpected(apply_refusal::invalid)) {
    return false;
  }
  if (settings.applied().get<limit_low>() != 10) return false;
  if (!settings.apply(upper, applier)) return false;
  if (!settings.apply(lower, applier)) return false;

  return settings.applied().get<limit_low>() == 25;
}

// The schema decides first: a parameter applied after a restart is refused
// whatever its value, and so is an index that names no parameter. With
// nothing applied on command left to apply, applying all succeeds without
// the applier, and a value waiting for a restart keeps waiting.
consteval bool test_the_schema_refuses_first()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  journal applier;

  if (settings.apply(pole_pairs, applier)
      != std::unexpected(apply_refusal::not_applicable)) {
    return false;
  }
  if (settings.last_refusal() != refusal{apply_refusal::not_applicable}) {
    return false;
  }
  if (settings.apply(image<schema>::count, applier)
      != std::unexpected(apply_refusal::unknown_parameter)) {
    return false;
  }

  if (!settings.set<motor_p>(std::int32_t{7})) return false;
  if (!settings.restart_required()) return false;
  if (settings.unapplied()) return false;
  if (!settings.apply_all(applier)) return false;
  if (!applier.calls.empty()) return false;
  if (settings.applied().get<motor_p>() != 11) return false;
  if (!settings.restart_required()) return false;

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

  if (!settings.discard(runout)) return false;
  if (settings.get<drive_runout>() != 100) return false;
  if (settings.get<motor_R>() != 0.5f) return false;
  auto const read_only = settings.discard(serial);
  if (read_only || (read_only.error() != error::read_only)) return false;
  auto const unknown = settings.discard(image<schema>::count);
  if (unknown || (unknown.error() != error::unknown_parameter)) return false;

  settings.discard_all();
  if (settings.get<motor_p>() != 11) return false;
  if (settings.get<motor_R>() != 0.0014f) return false;
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
// on command are applied, and checks the image, which holds the values that
// wait for a restart too.
consteval bool test_a_save_keeps_what_runs()
{
  fram memory;
  test_section settings;
  auto _ = settings.load(memory);
  journal applier;
  refusing refuses{true};

  if (!settings.set<drive_runout>(std::uint32_t{200})) return false;
  auto const unapplied = settings.save(refuses);
  if (unapplied || (unapplied.error().reason != save_refusal::unapplied)) {
    return false;
  }
  if (settings.last_refusal() != refusal{save_refusal::unapplied}) {
    return false;
  }
  if (settings.sequence() != 0) return false;

  if (!settings.apply_all(applier)) return false;
  if (!settings.set<motor_p>(std::int32_t{7})) return false;
  auto const invalid = settings.save(refuses);
  if (invalid || (invalid.error().reason != save_refusal::invalid)) {
    return false;
  }
  if (settings.last_refusal() != refusal{save_refusal::invalid}) return false;
  if (settings.sequence() != 0) return false;

  // The rules see a value that waits for a restart.
  if (!settings.set<motor_p>(std::int32_t{13})) return false;
  if (settings.save(applier)) return false;
  if (!settings.set<motor_p>(std::int32_t{7})) return false;

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
  if (settings.last_refusal() != refusal{save_refusal::medium}) return false;
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

// A rule that the values loaded break falls back: the defaults of its inputs
// become their values applied, while the values loaded stay requested, to be
// corrected or discarded, and a save waits with them. A rule that holds
// keeps its values, and so does every parameter that no broken rule reads.
consteval bool test_a_broken_rule_falls_back()
{
  fram memory;
  if (!store_limits(memory, 25, 22, 40)) return false;

  test_section restarted;
  auto _ = restarted.load(memory);
  if (restarted.fall_back_to_defaults(rules{})
      != std::array{true, false, false}) {
    return false;
  }
  auto const& applied = restarted.applied();
  if (applied.get<limit_low>() != 10) return false;
  if (applied.get<limit_high>() != 20) return false;
  if (applied.get<limit_top>() != 40) return false;
  if (restarted.get<limit_low>() != 25) return false;
  if (restarted.stored().get<limit_low>() != 25) return false;
  if (!restarted.unapplied()) return false;
  refusing accepts;
  if (restarted.save(accepts)) return false;

  restarted.discard_all();
  if (restarted.get<limit_low>() != 10) return false;
  return restarted.save(accepts).has_value();
}

// A fallback that moves an input another rule shares can break that rule,
// which then falls back in a pass of its own.
consteval bool test_a_fallback_can_break_another_rule()
{
  fram memory;
  if (!store_limits(memory, 25, 15, 17)) return false;

  test_section restarted;
  auto _ = restarted.load(memory);
  if (restarted.fall_back_to_defaults(rules{})
      != std::array{true, true, false}) {
    return false;
  }
  return restarted.applied().get<limit_top>() == 30;
}

// A pass checks every rule before it falls back any, so the order of the
// rules does not matter: here `ordered` falling back first would mend
// `capped`, and both fall back either way.
consteval bool test_the_order_of_the_rules_does_not_matter()
{
  fram memory;
  if (!store_limits(memory, 50, 40, 35)) return false;

  test_section first;
  auto _ = first.load(memory);
  if (first.fall_back_to_defaults(typelist<ordered, capped>{})
      != std::array{true, true}) {
    return false;
  }
  test_section second;
  auto _ = second.load(memory);
  if (second.fall_back_to_defaults(typelist<capped, ordered>{})
      != std::array{true, true}) {
    return false;
  }
  for (auto i = 0uz; i < image<schema>::count; ++i) {
    if (first.applied().cell_at(i) != second.applied().cell_at(i)) return false;
  }
  return first.applied().get<limit_top>() == 30;
}

// A rule over a parameter applied after a restart falls back as well: what
// derives from it is built from the default, and the value loaded owes the
// restart that would load it.
consteval bool test_a_restart_input_falls_back()
{
  fram memory;
  {
    test_section settings;
    auto _ = settings.load(memory);
    refusing accepts;
    if (!settings.set<motor_p>(std::int32_t{13})) return false;
    if (!settings.save(accepts)) return false;
  }

  test_section restarted;
  auto _ = restarted.load(memory);
  if (restarted.fall_back_to_defaults(rules{})
      != std::array{false, false, true}) {
    return false;
  }
  if (restarted.applied().get<motor_p>() != 11) return false;
  if (restarted.get<motor_p>() != 13) return false;
  if (restarted.unapplied()) return false;
  return restarted.restart_required();
}

// Without the fallback, values loaded that break a rule hold every change
// back: an application checks the values applied with the change taken in,
// and a save the image.
consteval bool test_a_broken_rule_holds_back_without_the_fallback()
{
  fram memory;
  if (!store_limits(memory, 25, 22, 40)) return false;

  test_section restarted;
  auto _ = restarted.load(memory);
  journal applier;
  if (!restarted.set<drive_runout>(std::uint32_t{200})) return false;
  if (restarted.apply_all(applier) != std::unexpected(apply_refusal::invalid)) {
    return false;
  }
  if (!restarted.discard(runout)) return false;
  auto const saved = restarted.save(applier);
  return !saved && (saved.error().reason == save_refusal::invalid);
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
  if (settings.requested().get<drive_runout>() != 200) return false;
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
  if (!settings.set<limit_top>(std::uint32_t{40})) return false;
  if (!settings.apply_all(applier)) return false;

  settings.restore_all_defaults();

  if (settings.get<drive_runout>() != 100) return false;
  if (settings.get<prod_serial>() != 7) return false;
  if (settings.get<motor_R>() != 0.0014f) return false;
  if (settings.get<limit_top>() != 30) return false;
  // The defaults are requested, not applied.
  if (!settings.unapplied()) return false;
  if (settings.applied().get<drive_runout>() != 300) return false;
  if (!settings.restart_required()) return false;

  return true;
}

static_assert(test_load_from_an_empty_medium());
static_assert(test_a_write_waits_until_applied());
static_assert(test_applying_is_all_or_nothing());
static_assert(test_a_parameter_applies_alone());
static_assert(test_the_schema_refuses_first());
static_assert(test_discard_puts_back_what_is_applied());
static_assert(test_unapplied_and_restart_required_split());
static_assert(test_an_unchanged_write_owes_nothing());
static_assert(test_a_save_keeps_what_runs());
static_assert(test_a_saved_restart_stays_owed());
static_assert(test_save_and_restart());
static_assert(test_a_load_takes_what_it_restored_as_applied());
static_assert(test_a_broken_rule_falls_back());
static_assert(test_a_fallback_can_break_another_rule());
static_assert(test_the_order_of_the_rules_does_not_matter());
static_assert(test_a_restart_input_falls_back());
static_assert(test_a_broken_rule_holds_back_without_the_fallback());
static_assert(test_wipe_leaves_the_image());
static_assert(test_sequence_follows_the_medium());
static_assert(test_access_by_index());
static_assert(test_the_images_part_and_meet());
static_assert(test_restore_all_defaults_spares_the_read_only());

} // namespace
