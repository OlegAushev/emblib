#include <array>
#include <type_traits>

#include <emb/math/clamped.hpp>
#include <emb/settings/schema.hpp>
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
           .max = std::int32_t{64}});
// A clamped brings its own bounds: nothing to spell out.
inline constexpr auto drive_stopping_torque =
    param("drive.stopping_torque",
          pu{0.05f},
          {.title = {"Stopping torque", "Момент остановки"},
           .apply = apply_policy::on_command});
inline constexpr auto prot_watchdog_timeout =
    param("prot.watchdog_timeout",
          std::uint32_t{1000},
          {.max = std::uint32_t{60000},
           .writable = false,
           .expose = false});

inline constexpr auto schema = make_schema(drive_phase_swap,
                                           drive_runout_speed,
                                           motor_p,
                                           drive_stopping_torque,
                                           prot_watchdog_timeout);

// -- Shape --

static_assert(schema.count == 5);
static_assert(
    std::same_as<schema_t<schema>::types,
                 typelist<bool, rpm, std::int32_t, pu, std::uint32_t>>);

// -- Lookup by name --

static_assert(schema.index_of("drive.phase_swap") == 0);
static_assert(schema.index_of("motor.p") == 2);
static_assert(schema.index_of("prot.watchdog_timeout") == 4);
static_assert(!schema.index_of("nope"));

// -- Static types recovered --

static_assert(std::same_as<type_at<schema, 0>, bool>);
static_assert(std::same_as<type_at<schema, 1>, rpm>);
static_assert(std::same_as<type_at<schema, 3>, pu>);

using speed = parameter<schema, drive_runout_speed>;
static_assert(speed::index == 1);
static_assert(std::same_as<speed::type, rpm>);
static_assert(speed::default_value == rpm{100.0f});
static_assert(parameter<schema, motor_p>::default_value == 11);
static_assert(parameter<schema, drive_phase_swap>::default_value == false);
static_assert(parameter<schema, drive_stopping_torque>::default_value
              == pu{0.05f});

// -- Descriptor contents --

static_assert(schema.parameters[1].kind == scalar_kind::float32);
static_assert(schema.parameters[1].min == to_cell(0.0f));
static_assert(schema.parameters[1].max == to_cell(5000.0f));
static_assert(schema.parameters[1].apply == apply_policy::on_command);
static_assert(schema.parameters[1].writable);
static_assert(schema.parameters[1].expose);

static_assert(schema.parameters[2].kind == scalar_kind::int32);
static_assert(schema.parameters[2].apply == apply_policy::on_restart);

// A wrapper's own limits become the parameter's bounds.
static_assert(schema.parameters[3].min == to_cell(0.0f));
static_assert(schema.parameters[3].max == to_cell(1.0f));

// Bounds left out are the widest the type admits.
static_assert(schema.parameters[4].min == to_cell(std::uint32_t{0}));
static_assert(schema.parameters[4].max == to_cell(std::uint32_t{60000}));
static_assert(!schema.parameters[4].writable);
static_assert(!schema.parameters[4].expose);

// -- Identifiers --

consteval bool test_ids_are_unique_and_sorted()
{
  for (auto i = 1uz; i < schema.count; ++i) {
    auto const prev = schema.parameters[schema.by_id[i - 1]].id;
    auto const cur = schema.parameters[schema.by_id[i]].id;
    if (prev >= cur) return false;
  }
  return true;
}

// The same name with a different type is a different parameter as far as
// stored data is concerned.
inline constexpr auto as_float =
    make_schema(param("x", 1.0f, {.title = {"X", "X"}}));
inline constexpr auto as_int =
    make_schema(param("x", std::int32_t{1}, {.title = {"X", "X"}}));
static_assert(as_float.parameters[0].id != as_int.parameters[0].id);

consteval bool test_find_by_id()
{
  for (auto i = 0uz; i < schema.count; ++i)
    if (schema.find(schema.parameters[i].id) != i) return false;

  // An identifier from another schema belongs to nothing here.
  if (schema.find(as_float.parameters[0].id)) return false;
  if (schema.find(0u)) return false;

  return true;
}

consteval bool test_single_parameter_schema()
{
  if (as_float.count != 1) return false;
  if (as_float.find(as_float.parameters[0].id) != 0) return false;
  if (as_float.find(as_float.parameters[0].id + 1)) return false;
  return true;
}

static_assert(test_ids_are_unique_and_sorted());
static_assert(test_find_by_id());
static_assert(test_single_parameter_schema());

// -- Title and unit --

// The declaration keeps the title and the unit for people; the unit is the
// symbol of the type unless the declaration gives another.
static_assert(drive_runout_speed.title.en == "Runout speed");
static_assert(drive_runout_speed.title.ru == "Скорость выбега");
static_assert(drive_runout_speed.unit == "rpm");
static_assert(drive_stopping_torque.unit == "pu");
static_assert(motor_p.unit.empty());
static_assert(drive_phase_swap.unit.empty());

// A unit replaces the symbol of a type that names none, or one that does not
// describe the parameter.
inline constexpr auto dc_undervoltage =
    param("protection.uvp_dc",
          35.0f,
          {.title = {"DC undervoltage", "Пониженное напряжение"}, .unit = "V"});
inline constexpr auto speed_slope =
    param("model.speed_slope",
          rpm{1000.0f},
          {.title = {"Speed slope", "Скорость нарастания скорости"},
           .unit = "rpm/s"});
static_assert(dc_undervoltage.unit == "V");
static_assert(speed_slope.unit == "rpm/s");

// A hidden parameter needs no title.
static_assert(prot_watchdog_timeout.title.en.empty());

// The title and the unit stay out of the descriptor, which the schema keeps:
// declarations that differ in them alone describe the same parameter.
inline constexpr auto renamed_for_people =
    param("drive.stopping_torque",
          pu{0.05f},
          {.title = {"Torque while stopping", "Момент при остановке"},
           .apply = apply_policy::on_command});
static_assert(renamed_for_people.desc == drive_stopping_torque.desc);

} // namespace
