#include <type_traits>

#include <emb/math/clamped.hpp>
#include <emb/settings/schema.hpp>
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
           .group = group::model}),
    // A clamped brings its own bounds: nothing to spell out.
    param("drive.stopping_torque",
          pu{0.05f},
          {.group = group::drive, .apply = apply_policy::live}),
    param("prot.watchdog_timeout",
          std::uint32_t{1000},
          {.max = std::uint32_t{60000},
           .group = group::drive,
           .writable = false,
           .expose = false}));

// -- Shape --

static_assert(schema.count == 5);
static_assert(std::same_as<schema_t<schema>::types,
                           typelist<bool, rpm, std::int32_t, pu,
                                    std::uint32_t>>);

// -- Lookup by name --

static_assert(schema.index_of("drive.phase_swap") == 0);
static_assert(schema.index_of("motor.p") == 2);
static_assert(schema.index_of("prot.watchdog_timeout") == 4);
static_assert(schema.index_of("nope") == schema_t<schema>::npos);

// -- Static types recovered --

static_assert(std::same_as<type_at<schema, 0>, bool>);
static_assert(std::same_as<type_at<schema, 1>, rpm>);
static_assert(std::same_as<type_at<schema, 3>, pu>);

using speed = parameter<schema, "drive.runout_speed">;
static_assert(speed::index == 1);
static_assert(std::same_as<speed::type, rpm>);
static_assert(speed::default_value == rpm{100.0f});
static_assert(parameter<schema, "motor.p">::default_value == 11);
static_assert(parameter<schema, "drive.phase_swap">::default_value == false);
static_assert(parameter<schema, "drive.stopping_torque">::default_value
              == pu{0.05f});

// -- Descriptor contents --

static_assert(schema.parameters[1].type == value_type::float32);
static_assert(schema.parameters[1].min == to_raw(0.0f));
static_assert(schema.parameters[1].max == to_raw(5000.0f));
static_assert(schema.parameters[1].apply == apply_policy::live);
static_assert(schema.parameters[1].group == group_id{group::drive});
static_assert(schema.parameters[1].writable);
static_assert(schema.parameters[1].expose);

static_assert(schema.parameters[2].type == value_type::int32);
static_assert(schema.parameters[2].group == group_id{group::model});
static_assert(schema.parameters[2].apply == apply_policy::on_restart);

// A wrapper's own limits become the parameter's bounds.
static_assert(schema.parameters[3].min == to_raw(0.0f));
static_assert(schema.parameters[3].max == to_raw(1.0f));

// Bounds left out are the widest the type admits.
static_assert(schema.parameters[4].min == to_raw(std::uint32_t{0}));
static_assert(schema.parameters[4].max == to_raw(std::uint32_t{60000}));
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
inline constexpr auto as_float = make_schema(param("x", 1.0f));
inline constexpr auto as_int = make_schema(param("x", std::int32_t{1}));
static_assert(as_float.parameters[0].id != as_int.parameters[0].id);

consteval bool test_find_by_id()
{
  for (auto i = 0uz; i < schema.count; ++i)
    if (schema.find(schema.parameters[i].id) != i) return false;

  // An identifier from another schema belongs to nothing here.
  if (schema.find(as_float.parameters[0].id) != schema_t<schema>::npos)
    return false;
  if (schema.find(0u) != schema_t<schema>::npos) return false;

  return true;
}

consteval bool test_single_parameter_schema()
{
  if (as_float.count != 1) return false;
  if (as_float.find(as_float.parameters[0].id) != 0) return false;
  if (as_float.find(as_float.parameters[0].id + 1)
      != schema_t<as_float>::npos) {
    return false;
  }
  return true;
}

static_assert(test_ids_are_unique_and_sorted());
static_assert(test_find_by_id());
static_assert(test_single_parameter_schema());

} // namespace
