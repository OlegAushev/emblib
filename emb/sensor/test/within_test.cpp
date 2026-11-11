#include <emb/sensor/within.hpp>

#include <emb/sensor/signalpath.hpp>
#include <emb/sensor/validated.hpp>
#include <emb/signal/affine.hpp>
#include <emb/signal/bind.hpp>
#include <emb/units.hpp>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace {

using namespace emb::units;
using emb::sensor::raw_range;
using emb::sensor::raw_range_of;
using emb::sensor::fault;
using emb::sensor::within;

// a 12-bit converter over a 3.3 V reference that saturates at its rails, as
// an ADC does
struct adc {
  static constexpr std::uint16_t forward(volt_f32 v)
  {
    return static_cast<std::uint16_t>(
        std::clamp(v.value * 4096.f / 3.3f, 0.f, 4095.f));
  }

  static constexpr volt_f32 inverse(std::uint16_t code)
  {
    return volt_f32{code * 3.3f / 4096.f};
  }
};

// a thermistor at the bottom of a divider: the pin falls as it heats up
inline constexpr emb::signal::affine thermistor{degree_celsius_f32{-40.f},
                                                degree_celsius_f32{150.f},
                                                volt_f32{3.0f},
                                                volt_f32{0.3f}};

using pin = emb::sensor::signalpath<adc>;
using thermistor_path =
    emb::sensor::signalpath<emb::signal::bind<thermistor>, adc>;

inline constexpr raw_range<std::uint16_t> healthy{100, 3900};
using guard = within<healthy>;

// called as const on a code, it returns the verdict, as validated asks
static_assert(std::invocable<guard const&, std::uint16_t const&>);
static_assert(
    std::same_as<std::invoke_result_t<guard const&, std::uint16_t const&>,
                 std::optional<fault>>);

// a range judges the raw values of its own type alone
static_assert(!std::invocable<guard const&, std::uint32_t const&>);
static_assert(!std::invocable<guard const&, int const&>);

// it is fixed at compile time and holds nothing
static_assert(std::is_empty_v<within<healthy>>);

// its ends belong to it
static_assert(within<healthy>{}(std::uint16_t{99}) == fault::below);
static_assert(within<healthy>{}(std::uint16_t{100}) == std::nullopt);
static_assert(within<healthy>{}(std::uint16_t{3900}) == std::nullopt);
static_assert(within<healthy>{}(std::uint16_t{3901}) == fault::above);

// a range of one value holds that value, and one turned round is refused
template<raw_range Range>
constexpr bool judges = requires { typename within<Range>; };

static_assert(judges<raw_range<std::uint16_t>{5, 5}>);
static_assert(!judges<raw_range<std::uint16_t>{6, 5}>);

constexpr bool test_range_of_a_rising_path()
{
  constexpr pin path;
  constexpr volt_f32 low{0.33f};
  constexpr volt_f32 high{2.97f};

  auto const range = raw_range_of(path, low, high);
  if (range.min != path.forward(low) || range.max != path.forward(high)) {
    return false;
  }

  // the order of the ends does not matter
  return raw_range_of(path, high, low) == range;
}

constexpr bool test_range_of_a_falling_path()
{
  constexpr thermistor_path path;
  constexpr degree_celsius_f32 cold{-20.f};
  constexpr degree_celsius_f32 hot{100.f};

  // the hot end gives the low code
  auto const range = raw_range_of(path, cold, hot);
  return range.min == path.forward(hot)
      && range.max == path.forward(cold)
      && raw_range_of(path, hot, cold) == range;
}

static_assert(test_range_of_a_rising_path());
static_assert(test_range_of_a_falling_path());

// an ADC saturates, so an end beyond its scale lands on a rail
static_assert(raw_range_of(pin{}, volt_f32{-1.f}, volt_f32{5.f})
              == raw_range<std::uint16_t>{0, 4095});

// A range computed from temperatures judges at compile time: a shorted
// thermistor reads hot, below the range, and an open one cold, above it.
using healthy_thermistor = within<raw_range_of(thermistor_path{},
                                               degree_celsius_f32{-20.f},
                                               degree_celsius_f32{100.f})>;

constexpr std::uint16_t code_at(float celsius)
{
  return thermistor_path{}.forward(degree_celsius_f32{celsius});
}

static_assert(healthy_thermistor{}(code_at(25.f)) == std::nullopt);
static_assert(healthy_thermistor{}(code_at(120.f)) == fault::below);
static_assert(healthy_thermistor{}(code_at(-30.f)) == fault::above);

} // namespace
