#include <type_traits>

#include <emb/math/clamped.hpp>
#include <emb/settings/image.hpp>
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
static_assert(std::same_as<
              decltype(std::declval<img const&>().get<"drive.runout_speed">()),
              rpm>);
static_assert(
    std::same_as<decltype(std::declval<img const&>().get<"motor.p">()),
                 std::int32_t>);

// -- Access by name --

consteval bool test_typed_access()
{
  img values;

  if (!values.set<"drive.runout_speed">(rpm{250.0f})) return false;
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
  // Closed comes before the type: a closed parameter refuses any value.
  auto const closed_float = values.set_at(serial, value{1.0f});
  if (closed_float || closed_float.error() != error::read_only) return false;

  auto const out = values.set_at(speed, value{9000.0f});
  if (out || out.error() != error::out_of_range) return false;

  auto const nothing = values.set_at(img::count, value{1.0f});
  if (nothing || nothing.error() != error::unknown_parameter) return false;
  if (values.get_at(img::count).has_value()) return false;

  if (!values.restore_default_at(speed)) return false;
  if (values.get<"drive.runout_speed">() != rpm{100.0f}) return false;

  // Nor may a closed parameter be put back to its default: restoring is a
  // write like any other, and what production recorded has to survive it.
  if (!values.set<"prod.serial">(std::uint32_t{12345})) return false;
  auto const reset = values.restore_default_at(serial);
  if (reset || reset.error() != error::read_only) return false;
  if (values.get<"prod.serial">() != 12345u) return false;

  return true;
}

// -- Unchanged writes --

consteval bool test_unchanged_write()
{
  img values;
  constexpr auto speed = *schema.index_of("drive.runout_speed");
  constexpr auto held = to_raw(rpm{100.0f});

  // Writing what a cell already holds is accepted and leaves the cell as it
  // was, whichever path the write takes.
  if (!values.set<"drive.runout_speed">(rpm{100.0f})) return false;
  if (values.cell(speed) != held) return false;
  if (!values.set_at(speed, value{100.0f})) return false;
  if (values.cell(speed) != held) return false;
  if (!values.restore_default<"drive.runout_speed">()) return false;
  if (values.cell(speed) != held) return false;
  if (!values.restore_default_at(speed)) return false;
  if (values.cell(speed) != held) return false;

  // A value that differs is written; the same value again is accepted.
  if (!values.set<"motor.p">(std::int32_t{4})) return false;
  if (values.get<"motor.p">() != 4) return false;
  if (!values.set<"motor.p">(std::int32_t{4})) return false;
  if (values.get<"motor.p">() != 4) return false;

  // An out-of-range value is refused, and the cell keeps its value.
  auto const out = values.set<"motor.p">(std::int32_t{65});
  if (out || out.error() != error::out_of_range) return false;
  if (values.get<"motor.p">() != 4) return false;

  return true;
}

// -- Cells --

consteval bool test_cells()
{
  img values;
  constexpr auto p = *schema.index_of("motor.p");

  if (values.cell(p) != to_raw(std::int32_t{11})) return false;

  // A loader assigns without validation.
  values.assign_cell(p, to_raw(std::int32_t{4}));
  if (values.get<"motor.p">() != 4) return false;

  if (!values.set<"motor.p">(std::int32_t{8})) return false;
  values.restore_defaults();
  if (values.get<"motor.p">() != 11) return false;

  return true;
}

static_assert(test_defaults());
static_assert(test_typed_access());
static_assert(test_erased_access());
static_assert(test_unchanged_write());
static_assert(test_cells());

} // namespace
