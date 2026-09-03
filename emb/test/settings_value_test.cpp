#include <optional>
#include <type_traits>
#include <variant>

#include <emb/math/clamped.hpp>
#include <emb/settings/value.hpp>
#include <emb/units.hpp>

namespace {

using namespace emb;
using namespace emb::settings;

using rpm = units::rpm_f32;
using pu = unsigned_pu_f32;

// -- Parameter types --

static_assert(some_parameter_type<bool>);
static_assert(some_parameter_type<std::int32_t>);
static_assert(some_parameter_type<std::uint32_t>);
static_assert(some_parameter_type<float>);

// Recognised structurally, not by name: a named_unit and a clamped go
// through the same path.
static_assert(some_parameter_type<rpm>);
static_assert(some_parameter_type<pu>);
static_assert(some_wrapped_value<rpm>);
static_assert(some_wrapped_value<pu>);

static_assert(!some_parameter_type<double>);
static_assert(!some_parameter_type<std::int8_t>);
static_assert(!some_parameter_type<std::int64_t>);
static_assert(!some_parameter_type<char const*>);

// The type-erased value is not itself a parameter type — which is what
// keeps to_raw(value) from being shadowed by the typed template.
static_assert(!some_parameter_type<value>);

struct wraps_a_double {
  using value_type = double;
  constexpr explicit wraps_a_double(double) {}
  constexpr double value() const { return 0.0; }
};

struct not_reconstructible {
  using value_type = float;
  constexpr float value() const { return 0.0f; }
};

struct value_type_disagrees {
  using value_type = float;
  constexpr explicit value_type_disagrees(float) {}
  constexpr double value() const { return 0.0; }
};

static_assert(!some_parameter_type<wraps_a_double>);
static_assert(!some_parameter_type<not_reconstructible>);
static_assert(!some_parameter_type<value_type_disagrees>);

// -- Type mapping --

static_assert(std::same_as<scalar_t<float>, float>);
static_assert(std::same_as<scalar_t<rpm>, float>);
static_assert(std::same_as<scalar_t<pu>, float>);

static_assert(type_of<bool> == value_type::boolean);
static_assert(type_of<std::int32_t> == value_type::int32);
static_assert(type_of<std::uint32_t> == value_type::uint32);
static_assert(type_of<float> == value_type::float32);
static_assert(type_of<rpm> == value_type::float32);
static_assert(type_of<pu> == value_type::float32);

static_assert(held_type(value{true}) == value_type::boolean);
static_assert(held_type(value{std::int32_t{-1}}) == value_type::int32);
static_assert(held_type(value{std::uint32_t{1}}) == value_type::uint32);
static_assert(held_type(value{1.0f}) == value_type::float32);

// -- Typed value <-> type-erased value --

consteval bool test_value_round_trip()
{
  if (from_value<bool>(to_value(true)) != true) return false;
  if (from_value<std::int32_t>(to_value(std::int32_t{-7})) != -7) return false;
  if (from_value<std::uint32_t>(to_value(std::uint32_t{7})) != 7u) return false;
  if (from_value<float>(to_value(2.5f)) != 2.5f) return false;

  // A wrapper is transported as its scalar and rebuilt from it.
  if (to_value(rpm{100.0f}) != value{100.0f}) return false;
  if (from_value<rpm>(value{100.0f}) != rpm{100.0f}) return false;
  if (from_value<pu>(value{0.5f}) != pu{0.5f}) return false;

  return true;
}

consteval bool test_value_type_mismatch()
{
  // A write carrying the wrong type is rejected, not reinterpreted.
  if (from_value<float>(value{std::int32_t{1}}).has_value()) return false;
  if (from_value<std::int32_t>(value{1.0f}).has_value()) return false;
  if (from_value<bool>(value{std::uint32_t{1}}).has_value()) return false;
  if (from_value<rpm>(value{std::int32_t{1}}).has_value()) return false;
  return true;
}

// -- Typed value <-> storage cell --

consteval bool test_raw_round_trip()
{
  if (from_raw<bool>(to_raw(true)) != true) return false;
  if (from_raw<bool>(to_raw(false)) != false) return false;
  if (from_raw<std::int32_t>(to_raw(std::int32_t{-7})) != -7) return false;
  if (from_raw<std::uint32_t>(to_raw(std::uint32_t{0xDEADBEEF})) != 0xDEADBEEF)
    return false;
  if (from_raw<float>(to_raw(-2.5f)) != -2.5f) return false;
  if (from_raw<rpm>(to_raw(rpm{1500.0f})) != rpm{1500.0f}) return false;
  if (from_raw<pu>(to_raw(pu{0.25f})) != pu{0.25f}) return false;
  return true;
}

consteval bool test_raw_bool_is_not_a_bit_cast()
{
  if (to_raw(true) != 1u) return false;
  if (to_raw(false) != 0u) return false;
  // Any non-zero cell reads as true: a corrupted cell cannot produce a bool
  // that is neither true nor false.
  if (from_raw<bool>(0xFFFFFFFFu) != true) return false;
  if (from_raw<bool>(2u) != true) return false;
  return true;
}

consteval bool test_erased_raw_conversions()
{
  if (to_raw(value{std::int32_t{-1}}) != 0xFFFFFFFFu) return false;
  if (to_raw(value{true}) != 1u) return false;

  if (to_value(value_type::int32, 0xFFFFFFFFu) != value{std::int32_t{-1}})
    return false;
  if (to_value(value_type::uint32, 0xFFFFFFFFu) != value{0xFFFFFFFFu})
    return false;
  if (to_value(value_type::boolean, 5u) != value{true}) return false;
  if (to_value(value_type::float32, to_raw(1.5f)) != value{1.5f}) return false;

  // A cell survives the round trip through the erased form.
  auto const v = to_value(value_type::float32, to_raw(rpm{750.0f}));
  if (from_value<rpm>(v) != rpm{750.0f}) return false;

  return true;
}

// -- Range checks --

consteval bool test_in_range()
{
  constexpr raw_value quiet_nan = 0x7FC00000u;

  // As a raw word, -5 is above any positive bound; as int32 it is not.
  auto const lo = to_raw(std::int32_t{-10});
  auto const hi = to_raw(std::int32_t{10});
  if (!in_range(value_type::int32, to_raw(std::int32_t{-5}), lo, hi))
    return false;
  if (in_range(value_type::int32, to_raw(std::int32_t{-11}), lo, hi))
    return false;
  if (in_range(value_type::int32, to_raw(std::int32_t{11}), lo, hi))
    return false;

  if (!in_range(value_type::uint32, 5u, 1u, 10u)) return false;
  if (in_range(value_type::uint32, 11u, 1u, 10u)) return false;

  auto const flo = to_raw(-1.0f);
  auto const fhi = to_raw(1.0f);
  if (!in_range(value_type::float32, to_raw(0.5f), flo, fhi)) return false;
  if (!in_range(value_type::float32, to_raw(-1.0f), flo, fhi)) return false;
  if (in_range(value_type::float32, to_raw(1.5f), flo, fhi)) return false;

  // A NaN restored from a corrupted cell is out of range whatever the
  // bounds are.
  if (in_range(value_type::float32, quiet_nan, flo, fhi)) return false;

  // A bool cell holds 0 or 1; the default bounds admit both.
  if (!in_range(value_type::boolean, to_raw(true), to_raw(false), to_raw(true)))
    return false;
  // Any other word in a bool cell is corruption, even though from_raw
  // would read it as true.
  if (in_range(value_type::boolean, 0xFFu, to_raw(false), to_raw(true)))
    return false;

  // less_equal is the primitive in_range is built from.
  if (!less_equal(value_type::int32, to_raw(std::int32_t{-5}), to_raw(5)))
    return false;
  if (less_equal(value_type::uint32, 6u, 5u)) return false;
  if (less_equal(value_type::float32, quiet_nan, to_raw(0.0f))) return false;
  if (less_equal(value_type::float32, to_raw(0.0f), quiet_nan)) return false;

  return true;
}

static_assert(test_value_round_trip());
static_assert(test_value_type_mismatch());
static_assert(test_raw_round_trip());
static_assert(test_raw_bool_is_not_a_bit_cast());
static_assert(test_erased_raw_conversions());
static_assert(test_in_range());

} // namespace
