#pragma once

#include <emb/sensor/validated.hpp>

#include <algorithm>
#include <concepts>
#include <optional>

namespace emb::sensor {

// The class template `bounds` holds two bounds, `min` and `max`, both
// included: those of the samples that a measurement chain gives while it is
// healthy. It is a structural type, so a `bounds` can be the template
// argument of `within`; `bounds_of` computes one from the quantities at the
// ends.
template<std::totally_ordered T>
struct bounds {
  T min;
  T max;

  friend constexpr bool operator==(bounds const&, bounds const&) = default;
};

// Returns the bounds of the values that the path `path` gives for the
// quantities from `a` to `b`: `path.forward(a)` and `path.forward(b)`, the
// lower of the two as `min`. The order of `a` and `b` does not matter, and a
// path whose value falls as the quantity rises, such as a thermistor at the
// bottom of a divider, gives its bounds the same way. The bounds hold the
// values of exactly the quantities between `a` and `b` if the path is
// monotonic over them, as a path of affine stages, thermistors and resistance
// thermometers is. Each bound is as accurate as the rounding of the last
// stage, and a stage that saturates, such as an ADC, puts a bound beyond its
// scale on its rail, which the bounds then hold too.
//
// A limit known at a node inside a path, such as the current of a 4-20 mA
// loop, is passed with the part of the path from that node on. `bounds_of` is
// `constexpr`, so bounds can be computed at compile time as the argument of
// `within`, or at run time for a validator that holds its bounds, e.g. from
// settings.
template<typename Path, typename Quantity>
  requires requires(Path const& path, Quantity q) {
    { path.forward(q) } -> std::totally_ordered;
  }
constexpr auto bounds_of(Path const& path, Quantity a, Quantity b)
{
  using value_type = decltype(path.forward(a));
  value_type const at_a = path.forward(a);
  value_type const at_b = path.forward(b);
  return bounds<value_type>{std::min(at_a, at_b), std::max(at_a, at_b)};
}

// The class template `within` is a validator that accepts the samples within
// the bounds `Bounds`, both included, and refuses a sample below `Bounds.min`
// with `fault::below` and a sample above `Bounds.max` with `fault::above`. It
// takes samples of the type of the bounds alone, so bounds computed for one
// type do not judge the samples of another, and bounds whose `min` exceeds
// their `max` do not compile. `Bounds` is fixed at compile time, so a
// `within` is empty and takes no storage in a `validated`; bounds set at run
// time, e.g. from settings, need a validator that holds them.
template<bounds Bounds>
  requires(!(Bounds.max < Bounds.min))
struct within {
  using sample_type = decltype(Bounds.min);

  constexpr std::optional<fault>
  operator()(std::same_as<sample_type> auto sample) const
  {
    if (sample < Bounds.min) {
      return fault::below;
    }
    if (Bounds.max < sample) {
      return fault::above;
    }
    return std::nullopt;
  }
};

} // namespace emb::sensor
