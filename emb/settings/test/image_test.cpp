#include <cstdint>
#include <span>
#include <type_traits>

#include <emb/math/clamped.hpp>
#include <emb/meta/enum_info.hpp>
#include <emb/settings/image.hpp>
#include <emb/units.hpp>

namespace {

using namespace emb;
using namespace emb::settings;

using rpm = units::rpm_f32;
using pu = unsigned_pu_f32;

inline constexpr auto drive_phase_swap =
    param("drive.phase_swap",
          false,
          {.title = {"Phase swap", "Перестановка фаз"}});
inline constexpr auto drive_runout_speed =
    param("drive.runout_speed",
          rpm{100.0f},
          {.title = {"Runout speed", "Скорость выбега"},
           .min = rpm{0.0f},
           .max = rpm{5000.0f},
           .apply = apply_policy::on_command});
inline constexpr auto motor_p =
    param("motor.p",
          std::int32_t{11},
          {.title = {"Pole pairs", "Число пар полюсов"},
           .min = std::int32_t{1},
           .max = std::int32_t{64},
           .apply = apply_policy::on_restart});
inline constexpr auto model_torque_slope =
    param("model.torque_slope",
          pu{0.5f},
          {.title = {"Torque slope", "Скорость нарастания момента"},
           .apply = apply_policy::on_command});
inline constexpr auto prod_serial =
    param("prod.serial",
          std::uint32_t{0},
          {.title = {"Serial number", "Серийный номер"}, .writable = false});

inline constexpr auto schema = make_schema(drive_phase_swap,
                                           drive_runout_speed,
                                           motor_p,
                                           model_torque_slope,
                                           prod_serial);

using img = image<schema>;

static_assert(img::count == 5);

// -- Defaults --

consteval bool test_defaults()
{
  img values;
  if (values.get<drive_phase_swap>() != false) return false;
  if (values.get<drive_runout_speed>() != rpm{100.0f}) return false;
  if (values.get<motor_p>() != 11) return false;
  if (values.get<model_torque_slope>() != pu{0.5f}) return false;
  return true;
}

// The static type survives the round trip through a cell.
static_assert(
    std::same_as<decltype(std::declval<img const&>().get<drive_runout_speed>()),
                 rpm>);
static_assert(std::same_as<decltype(std::declval<img const&>().get<motor_p>()),
                           std::int32_t>);

// -- Access by declaration --

consteval bool test_typed_access()
{
  img values;

  if (!values.set<drive_runout_speed>(rpm{250.0f})) return false;
  if (values.get<drive_runout_speed>() != rpm{250.0f}) return false;

  // Out of the declared range: refused, and the cell keeps its value.
  auto const refused = values.set<drive_runout_speed>(rpm{9000.0f});
  if (refused) return false;
  if (refused.error() != error::out_of_range) return false;
  if (values.get<drive_runout_speed>() != rpm{250.0f}) return false;

  if (!values.restore_default<drive_runout_speed>()) return false;
  if (values.get<drive_runout_speed>() != rpm{100.0f}) return false;

  // A parameter closed to a protocol is still writable by the code that
  // owns it.
  if (!values.set<prod_serial>(std::uint32_t{12345})) return false;
  if (values.get<prod_serial>() != 12345u) return false;

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
  if (values.get<drive_runout_speed>() != rpm{250.0f}) return false;

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
  if (values.get<drive_runout_speed>() != rpm{100.0f}) return false;

  // Nor may a closed parameter be put back to its default: restoring is a
  // write like any other, and what production recorded has to survive it.
  if (!values.set<prod_serial>(std::uint32_t{12345})) return false;
  auto const reset = values.restore_default_at(serial);
  if (reset || reset.error() != error::read_only) return false;
  if (values.get<prod_serial>() != 12345u) return false;

  return true;
}

// -- Unchanged writes --

consteval bool test_unchanged_write()
{
  img values;
  constexpr auto speed = *schema.index_of("drive.runout_speed");
  constexpr auto held = to_cell(rpm{100.0f});

  // Writing what a cell already holds is accepted and leaves the cell as it
  // was, whichever path the write takes.
  if (!values.set<drive_runout_speed>(rpm{100.0f})) return false;
  if (values.cell_at(speed) != held) return false;
  if (!values.set_at(speed, value{100.0f})) return false;
  if (values.cell_at(speed) != held) return false;
  if (!values.restore_default<drive_runout_speed>()) return false;
  if (values.cell_at(speed) != held) return false;
  if (!values.restore_default_at(speed)) return false;
  if (values.cell_at(speed) != held) return false;

  // A value that differs is written; the same value again is accepted.
  if (!values.set<motor_p>(std::int32_t{4})) return false;
  if (values.get<motor_p>() != 4) return false;
  if (!values.set<motor_p>(std::int32_t{4})) return false;
  if (values.get<motor_p>() != 4) return false;

  // An out-of-range value is refused, and the cell keeps its value.
  auto const out = values.set<motor_p>(std::int32_t{65});
  if (out || out.error() != error::out_of_range) return false;
  if (values.get<motor_p>() != 4) return false;

  return true;
}

// -- Cells --

consteval bool test_cells()
{
  img values;
  constexpr auto p = *schema.index_of("motor.p");

  if (values.cell_at(p) != to_cell(std::int32_t{11})) return false;

  // A loader assigns without validation.
  values.assign_cell(p, to_cell(std::int32_t{4}));
  if (values.get<motor_p>() != 4) return false;

  if (!values.set<motor_p>(std::int32_t{8})) return false;
  values.assign_defaults();
  if (values.get<motor_p>() != 11) return false;

  return true;
}

static_assert(test_defaults());
static_assert(test_typed_access());
static_assert(test_erased_access());
static_assert(test_unchanged_write());
static_assert(test_cells());

// -- Enumerations --

enum class pwm : std::uint8_t { svpwm, dpwm0, dpwm1 };

consteval std::span<enumerator_info<pwm> const> enum_info(pwm)
{
  static constexpr enumerator_info<pwm> info[] = {
      {pwm::svpwm, "svpwm", {"SVPWM", "SVPWM"}},
      {pwm::dpwm0, "dpwm0", {"DPWM0", "DPWM0"}},
      {pwm::dpwm1, "dpwm1", {"DPWM1", "DPWM1"}},
  };
  return info;
}

inline constexpr auto model_pwm_mode = param(
    "model.pwm_mode",
    pwm::dpwm0,
    {.title = {"PWM mode", "Режим ШИМ"}, .apply = apply_policy::on_command});

inline constexpr auto enum_schema = make_schema(model_pwm_mode);

// The code reads and writes the enumeration; a protocol writes its carrier,
// which the bounds check before the enumeration is formed from it.
consteval bool test_enumerations()
{
  image<enum_schema> values;
  if (values.get<model_pwm_mode>() != pwm::dpwm0) return false;
  if (!values.set<model_pwm_mode>(pwm::dpwm1)) return false;
  if (values.get<model_pwm_mode>() != pwm::dpwm1) return false;

  if (!values.set_at(0, value{std::uint32_t{0}})) return false;
  if (values.get<model_pwm_mode>() != pwm::svpwm) return false;
  auto const read = values.get_at(0);
  if (!read || *read != value{std::uint32_t{0}}) return false;

  // Past the bounds, and past a byte where its low byte names dpwm0.
  auto const past = values.set_at(0, value{std::uint32_t{3}});
  if (past || past.error() != error::out_of_range) return false;
  auto const wrapped = values.set_at(0, value{std::uint32_t{257}});
  if (wrapped || wrapped.error() != error::out_of_range) return false;
  auto const mismatch = values.set_at(0, value{std::int32_t{1}});
  if (mismatch || mismatch.error() != error::type_mismatch) return false;
  return values.get<model_pwm_mode>() == pwm::svpwm;
}

static_assert(test_enumerations());

} // namespace
