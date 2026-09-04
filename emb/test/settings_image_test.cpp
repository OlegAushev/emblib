#include <atomic>
#include <type_traits>

#include <emb/math/clamped.hpp>
#include <emb/settings/image.hpp>
#include <emb/settings/pending.hpp>
#include <emb/units.hpp>

namespace {

using namespace emb;
using namespace emb::settings;

using rpm = units::rpm_f32;
using pu = unsigned_pu_f32;

enum class group : std::uint8_t { drive, model };

inline constexpr auto schema = make_schema(
    param("drive.phase_swap", false, {.group = group::drive}),
    param("drive.runout_speed",
          rpm{100.0f},
          {.min = rpm{0.0f},
           .max = rpm{5000.0f},
           .group = group::drive,
           .apply = apply_policy::live}),
    param("motor.p",
          std::int32_t{11},
          {.min = std::int32_t{1},
           .max = std::int32_t{64},
           .group = group::model,
           .apply = apply_policy::on_restart}),
    param("model.torque_slope",
          pu{0.5f},
          {.group = group::model, .apply = apply_policy::on_safe_state}),
    param("prod.serial",
          std::uint32_t{0},
          {.group = group::drive, .writable = false}));

using img = image<schema>;

static_assert(img::count == 5);
static_assert(groups_fit<schema>());

// -- Defaults --

consteval bool test_defaults()
{
  img values;
  if (values.get<"drive.phase_swap">() != false) return false;
  if (values.get<"drive.runout_speed">() != rpm{100.0f}) return false;
  if (values.get<"motor.p">() != 11) return false;
  if (values.get<"model.torque_slope">() != pu{0.5f}) return false;
  return true;
}

// The static type survives the round trip through a cell.
static_assert(
    std::same_as<decltype(std::declval<img const&>().get<"drive.runout_speed">()),
                 rpm>);
static_assert(
    std::same_as<decltype(std::declval<img const&>().get<"motor.p">()),
                 std::int32_t>);

// -- Access by name --

consteval bool test_typed_access()
{
  img values;

  auto const written = values.set<"drive.runout_speed">(rpm{250.0f});
  if (!written) return false;
  if (*written != change{group_id{group::drive}, apply_policy::live}) {
    return false;
  }
  if (values.get<"drive.runout_speed">() != rpm{250.0f}) return false;

  // Out of the declared range: refused, and the cell keeps its value.
  auto const refused = values.set<"drive.runout_speed">(rpm{9000.0f});
  if (refused) return false;
  if (refused.error() != error::out_of_range) return false;
  if (values.get<"drive.runout_speed">() != rpm{250.0f}) return false;

  if (!values.restore_default<"drive.runout_speed">()) return false;
  if (values.get<"drive.runout_speed">() != rpm{100.0f}) return false;

  // A parameter closed to a protocol is still writable by the code that
  // owns it.
  if (!values.set<"prod.serial">(std::uint32_t{12345})) return false;
  if (values.get<"prod.serial">() != 12345u) return false;

  return true;
}

// -- Access by index --

consteval bool test_erased_access()
{
  img values;
  constexpr auto speed = *schema.index_of("drive.runout_speed");
  constexpr auto serial = *schema.index_of("prod.serial");

  auto const read = values.get_at(speed);
  if (!read || *read != value{100.0f}) return false;

  if (!values.set_at(speed, value{250.0f})) return false;
  if (values.get<"drive.runout_speed">() != rpm{250.0f}) return false;

  // A wrapper is written as its scalar; the wrong alternative is refused.
  auto const mismatch = values.set_at(speed, value{std::int32_t{250}});
  if (mismatch || mismatch.error() != error::type_mismatch) return false;

  auto const closed = values.set_at(serial, value{std::uint32_t{1}});
  if (closed || closed.error() != error::read_only) return false;

  auto const out = values.set_at(speed, value{9000.0f});
  if (out || out.error() != error::out_of_range) return false;

  auto const nothing = values.set_at(img::count, value{1.0f});
  if (nothing || nothing.error() != error::unknown_parameter) return false;
  if (values.get_at(img::count).has_value()) return false;

  if (!values.restore_default_at(speed)) return false;
  if (values.get<"drive.runout_speed">() != rpm{100.0f}) return false;

  return true;
}

// -- Cells --

consteval bool test_cells()
{
  img values;
  constexpr auto p = *schema.index_of("motor.p");

  if (values.cell(p) != to_raw(std::int32_t{11})) return false;

  // A loader assigns without validation and without reporting a change.
  values.assign_cell(p, to_raw(std::int32_t{4}));
  if (values.get<"motor.p">() != 4) return false;

  if (!values.set<"motor.p">(std::int32_t{8})) return false;
  values.restore_defaults();
  if (values.get<"motor.p">() != 11) return false;

  return true;
}

// -- Pending changes --
//
// The word type is swapped for a plain one so the bit arithmetic runs in a
// constant expression; std::atomic cannot.

struct plain_word {
  std::uint32_t value = 0;

  constexpr auto load(std::memory_order) const -> std::uint32_t
  {
    return value;
  }

  constexpr auto fetch_or(std::uint32_t bits, std::memory_order)
      -> std::uint32_t
  {
    auto const before = value;
    value |= bits;
    return before;
  }

  constexpr auto fetch_and(std::uint32_t bits, std::memory_order)
      -> std::uint32_t
  {
    auto const before = value;
    value &= bits;
    return before;
  }
};

using test_pending = basic_pending_changes<plain_word>;

consteval bool test_pending_is_taken_once()
{
  test_pending pending;
  constexpr group_id drive{group::drive};

  if (pending.take(drive, apply_policy::live)) return false;

  pending.mark(drive, apply_policy::live);
  if (!pending.changed(drive, apply_policy::live)) return false;
  if (!pending.take(drive, apply_policy::live)) return false;
  // Taken means cleared.
  if (pending.take(drive, apply_policy::live)) return false;
  if (pending.changed(drive, apply_policy::live)) return false;

  return true;
}

consteval bool test_pending_respects_the_policy()
{
  test_pending pending;
  constexpr group_id model{group::model};

  pending.mark(model, apply_policy::on_safe_state);

  // A caller that can only apply live changes leaves it waiting.
  if (pending.take(model, apply_policy::live)) return false;
  if (!pending.changed(model, apply_policy::on_safe_state)) return false;

  // One that can reconfigure safely takes it, and takes live changes too.
  pending.mark(model, apply_policy::live);
  if (!pending.take(model, apply_policy::on_safe_state)) return false;
  if (pending.changed(model, apply_policy::on_safe_state)) return false;

  return true;
}

consteval bool test_a_group_waiting_on_more_is_refused()
{
  test_pending pending;
  constexpr group_id model{group::model};

  pending.mark(model, apply_policy::live);
  pending.mark(model, apply_policy::on_safe_state);

  // A caller that can only apply live changes gets nothing: the group is
  // rebuilt whole, and half of it must wait for a safe state.
  if (pending.take(model, apply_policy::live)) return false;
  // And nothing was consumed on the way out.
  if (!pending.changed(model, apply_policy::live)) return false;

  // A caller that can honour both takes both.
  if (!pending.take(model, apply_policy::on_safe_state)) return false;
  if (pending.changed(model, apply_policy::on_safe_state)) return false;

  // A change that needs a restart refuses the group to everyone.
  pending.mark(model, apply_policy::live);
  pending.mark(model, apply_policy::on_restart);
  if (pending.take(model, apply_policy::on_safe_state)) return false;
  if (!pending.changed(model, apply_policy::live)) return false;

  return true;
}

consteval bool test_pending_groups_are_independent()
{
  test_pending pending;
  constexpr group_id drive{group::drive};
  constexpr group_id model{group::model};

  pending.mark(drive, apply_policy::live);
  pending.mark(model, apply_policy::live);

  if (!pending.take(drive, apply_policy::live)) return false;
  if (!pending.changed(model, apply_policy::live)) return false;
  if (pending.mask(apply_policy::live) != (1u << model.value)) return false;

  return true;
}

consteval bool test_restart_required()
{
  test_pending pending;
  constexpr group_id model{group::model};

  if (pending.restart_required()) return false;

  pending.mark(change{model, apply_policy::on_restart});
  if (!pending.restart_required()) return false;

  // Nothing a caller can honour, so nothing is taken and the condition
  // stands until the restart.
  if (pending.take(model, apply_policy::on_safe_state)) return false;
  if (!pending.restart_required()) return false;

  pending.clear();
  if (pending.restart_required()) return false;

  return true;
}

// The production instantiation must compile for the target too, not only
// the test double it is checked through.
[[maybe_unused]] void instantiate_atomic_pending()
{
  constexpr group_id drive{group::drive};
  pending_changes pending;
  pending.mark(change{drive, apply_policy::live});
  [[maybe_unused]] auto const taken = pending.take(drive, apply_policy::live);
  [[maybe_unused]] auto const waiting =
      pending.changed(drive, apply_policy::live);
  [[maybe_unused]] auto const bits = pending.mask(apply_policy::live);
  [[maybe_unused]] auto const restart = pending.restart_required();
  pending.clear();
}

static_assert(test_defaults());
static_assert(test_typed_access());
static_assert(test_erased_access());
static_assert(test_cells());
static_assert(test_pending_is_taken_once());
static_assert(test_pending_respects_the_policy());
static_assert(test_a_group_waiting_on_more_is_refused());
static_assert(test_pending_groups_are_independent());
static_assert(test_restart_required());

} // namespace
