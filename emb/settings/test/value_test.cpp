#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <variant>

#include <emb/math/clamped.hpp>
#include <emb/meta/enum_info.hpp>
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

// Recognised by a value_of overload that argument-dependent lookup finds,
// not by name: a quantity and a clamped go through the same path.
static_assert(some_parameter_type<rpm>);
static_assert(some_parameter_type<pu>);
static_assert(some_wrapped_value<rpm>);
static_assert(some_wrapped_value<pu>);

static_assert(!some_parameter_type<double>);
static_assert(!some_parameter_type<std::int8_t>);
static_assert(!some_parameter_type<std::int64_t>);
static_assert(!some_parameter_type<char const*>);

// The type-erased value is not itself a parameter type — which is what
// keeps to_cell(value) from being shadowed by the typed template.
static_assert(!some_parameter_type<value>);

struct wraps_a_double {
  using value_type = double;
  constexpr explicit wraps_a_double(double) {}
  friend constexpr double value_of(wraps_a_double)
  {
    return 0.0;
  }
};

struct not_reconstructible {
  using value_type = float;
  friend constexpr float value_of(not_reconstructible)
  {
    return 0.0f;
  }
};

struct value_type_disagrees {
  using value_type = float;
  constexpr explicit value_type_disagrees(float) {}
  friend constexpr double value_of(value_type_disagrees)
  {
    return 0.0;
  }
};

// A value() member without value_of does not make a wrapper.
struct value_member_only {
  using value_type = float;
  constexpr explicit value_member_only(float) {}
  constexpr float value() const
  {
    return 0.0f;
  }
};

static_assert(!some_parameter_type<wraps_a_double>);
static_assert(!some_parameter_type<not_reconstructible>);
static_assert(!some_parameter_type<value_type_disagrees>);
static_assert(!some_parameter_type<value_member_only>);

// Nor does a wrapper need a value() member: value_of may read a public
// field, which is where a structural type keeps its number.
struct public_field {
  using value_type = float;
  float v;
  constexpr explicit public_field(float x) : v(x) {}
  friend constexpr float value_of(public_field w)
  {
    return w.v;
  }
};

static_assert(some_parameter_type<public_field>);

// -- Type mapping --

static_assert(std::same_as<scalar_t<float>, float>);
static_assert(std::same_as<scalar_t<rpm>, float>);
static_assert(std::same_as<scalar_t<pu>, float>);

static_assert(kind_of<bool> == scalar_kind::boolean);
static_assert(kind_of<std::int32_t> == scalar_kind::int32);
static_assert(kind_of<std::uint32_t> == scalar_kind::uint32);
static_assert(kind_of<float> == scalar_kind::float32);
static_assert(kind_of<rpm> == scalar_kind::float32);
static_assert(kind_of<pu> == scalar_kind::float32);

static_assert(held_kind(value{true}) == scalar_kind::boolean);
static_assert(held_kind(value{std::int32_t{-1}}) == scalar_kind::int32);
static_assert(held_kind(value{std::uint32_t{1}}) == scalar_kind::uint32);
static_assert(held_kind(value{1.0f}) == scalar_kind::float32);

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
  if (to_value(public_field{2.5f}) != value{2.5f}) return false;

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
  if (from_cell<bool>(to_cell(true)) != true) return false;
  if (from_cell<bool>(to_cell(false)) != false) return false;
  if (from_cell<std::int32_t>(to_cell(std::int32_t{-7})) != -7) return false;
  if (from_cell<std::uint32_t>(to_cell(std::uint32_t{0xDEADBEEF}))
      != 0xDEADBEEF)
    return false;
  if (from_cell<float>(to_cell(-2.5f)) != -2.5f) return false;
  if (from_cell<rpm>(to_cell(rpm{1500.0f})) != rpm{1500.0f}) return false;
  if (from_cell<pu>(to_cell(pu{0.25f})) != pu{0.25f}) return false;
  if (to_cell(public_field{-2.5f}) != to_cell(-2.5f)) return false;
  return true;
}

consteval bool test_raw_bool_is_not_a_bit_cast()
{
  if (to_cell(true) != 1u) return false;
  if (to_cell(false) != 0u) return false;
  // Any non-zero cell reads as true: a corrupted cell cannot produce a bool
  // that is neither true nor false.
  if (from_cell<bool>(0xFFFFFFFFu) != true) return false;
  if (from_cell<bool>(2u) != true) return false;
  return true;
}

consteval bool test_erased_raw_conversions()
{
  if (to_cell(value{std::int32_t{-1}}) != 0xFFFFFFFFu) return false;
  if (to_cell(value{true}) != 1u) return false;

  if (to_value(scalar_kind::int32, 0xFFFFFFFFu) != value{std::int32_t{-1}})
    return false;
  if (to_value(scalar_kind::uint32, 0xFFFFFFFFu) != value{0xFFFFFFFFu})
    return false;
  if (to_value(scalar_kind::boolean, 5u) != value{true}) return false;
  if (to_value(scalar_kind::float32, to_cell(1.5f)) != value{1.5f})
    return false;

  // A cell survives the round trip through the erased form.
  auto const v = to_value(scalar_kind::float32, to_cell(rpm{750.0f}));
  if (from_value<rpm>(v) != rpm{750.0f}) return false;

  return true;
}

// -- Range checks --

consteval bool test_in_range()
{
  constexpr cell quiet_nan = 0x7FC00000u;

  // As a raw word, -5 is above any positive bound; as int32 it is not.
  auto const lo = to_cell(std::int32_t{-10});
  auto const hi = to_cell(std::int32_t{10});
  if (!in_range(scalar_kind::int32, to_cell(std::int32_t{-5}), lo, hi))
    return false;
  if (in_range(scalar_kind::int32, to_cell(std::int32_t{-11}), lo, hi))
    return false;
  if (in_range(scalar_kind::int32, to_cell(std::int32_t{11}), lo, hi))
    return false;

  if (!in_range(scalar_kind::uint32, 5u, 1u, 10u)) return false;
  if (in_range(scalar_kind::uint32, 11u, 1u, 10u)) return false;

  auto const flo = to_cell(-1.0f);
  auto const fhi = to_cell(1.0f);
  if (!in_range(scalar_kind::float32, to_cell(0.5f), flo, fhi)) return false;
  if (!in_range(scalar_kind::float32, to_cell(-1.0f), flo, fhi)) return false;
  if (in_range(scalar_kind::float32, to_cell(1.5f), flo, fhi)) return false;

  // A NaN restored from a corrupted cell is out of range whatever the
  // bounds are.
  if (in_range(scalar_kind::float32, quiet_nan, flo, fhi)) return false;

  // A bool cell holds 0 or 1; the default bounds admit both.
  if (!in_range(scalar_kind::boolean,
                to_cell(true),
                to_cell(false),
                to_cell(true)))
    return false;
  // Any other word in a bool cell is corruption, even though from_cell
  // would read it as true.
  if (in_range(scalar_kind::boolean, 0xFFu, to_cell(false), to_cell(true)))
    return false;

  // less_equal is the primitive in_range is built from.
  if (!less_equal(scalar_kind::int32, to_cell(std::int32_t{-5}), to_cell(5)))
    return false;
  if (less_equal(scalar_kind::uint32, 6u, 5u)) return false;
  if (less_equal(scalar_kind::float32, quiet_nan, to_cell(0.0f))) return false;
  if (less_equal(scalar_kind::float32, to_cell(0.0f), quiet_nan)) return false;

  return true;
}

static_assert(test_value_round_trip());
static_assert(test_value_type_mismatch());
static_assert(test_raw_round_trip());
static_assert(test_raw_bool_is_not_a_bit_cast());
static_assert(test_erased_raw_conversions());
static_assert(test_in_range());

// -- Enumerations --

// A mode, its values labelled with a gap before the last one.
enum class mode : std::uint8_t { off, slow, fast, turbo = 7 };

consteval std::span<enumerator_info<mode> const> enum_info(mode)
{
  static constexpr enumerator_info<mode> info[] = {
      {mode::off, "off", {"Off", "Выкл."}},
      {mode::slow, "slow", {"Slow", "Медленно"}},
      {mode::fast, "fast", {"Fast", "Быстро"}},
      {mode::turbo, "turbo", {"Turbo", "Форсаж"}},
  };
  return info;
}

// A tilt, below zero: its underlying type is signed.
enum class tilt : std::int16_t { left = -1, level, right };

consteval std::span<enumerator_info<tilt> const> enum_info(tilt)
{
  static constexpr enumerator_info<tilt> info[] = {
      {tilt::left, "left", {"Left", "Влево"}},
      {tilt::level, "level", {"Level", "Ровно"}},
      {tilt::right, "right", {"Right", "Вправо"}},
  };
  return info;
}

// Labelled, but too wide for a cell; and not labelled.
enum class wide : std::uint64_t { a };

consteval std::span<enumerator_info<wide> const> enum_info(wide)
{
  static constexpr enumerator_info<wide> info[] = {{wide::a, "a", {"A", "А"}}};
  return info;
}

enum class unlabelled : std::uint8_t { a };

// A parameter may be an enumeration that labels its values and fits a cell;
// it is stored as its carrier, of the signedness of its underlying type.
static_assert(some_enum_value<mode>);
static_assert(some_parameter_type<mode>);
static_assert(some_parameter_type<tilt>);
static_assert(!some_parameter_type<wide>);
static_assert(!some_parameter_type<unlabelled>);
static_assert(std::same_as<scalar_t<mode>, std::uint32_t>);
static_assert(std::same_as<scalar_t<tilt>, std::int32_t>);
static_assert(kind_of<mode> == scalar_kind::uint32);
static_assert(kind_of<tilt> == scalar_kind::int32);

consteval bool test_enumerations()
{
  if (to_value(mode::turbo) != value{std::uint32_t{7}}) return false;
  if (to_value(tilt::left) != value{std::int32_t{-1}}) return false;
  if (from_value<mode>(value{std::uint32_t{7}}) != mode::turbo) return false;
  if (from_value<tilt>(value{std::int32_t{-1}}) != tilt::left) return false;
  // A value outside the underlying type is not cut to its width, nor is
  // another scalar converted.
  if (from_value<mode>(value{std::uint32_t{263}})) return false;
  if (from_value<tilt>(value{std::int32_t{40000}})) return false;
  if (from_value<mode>(value{std::int32_t{7}})) return false;

  if (to_cell(mode::turbo) != 7u) return false;
  if (to_cell(tilt::left) != 0xFFFFFFFFu) return false;
  if (from_cell<mode>(to_cell(mode::fast)) != mode::fast) return false;
  if (from_cell<tilt>(to_cell(tilt::left)) != tilt::left) return false;
  return to_value(scalar_kind::uint32, to_cell(mode::turbo))
      == to_value(mode::turbo);
}

static_assert(test_enumerations());

} // namespace
