#pragma once

#include <emb/sensor/validated.hpp>

#include <algorithm>
#include <concepts>
#include <optional>

namespace emb::sensor {

// The class template `raw_range` is the range of raw values from `min` to
// `max`, both included, that a measurement chain gives while it is healthy.
// It is a structural type, so a `raw_range` can be the template argument of
// `within`; `raw_range_of` computes one from the quantities at its ends.
template<std::totally_ordered Raw>
struct raw_range {
  Raw min;
  Raw max;

  friend constexpr bool operator==(raw_range const&,
                                   raw_range const&) = default;
};

// Returns the range of the raw values that the path `path` gives for the
// quantities from `a` to `b`: `path.forward(a)` and `path.forward(b)`, the
// lower of the two as `min`. The order of `a` and `b` does not matter, and a
// path whose raw value falls as the quantity rises, such as a thermistor at
// the bottom of a divider, gives its range the same way. The range holds the
// raw values of exactly the quantities between `a` and `b` if the path is
// monotonic over them, as a path of affine stages, thermistors and resistance
// thermometers is. Each end is as accurate as the rounding of the last stage,
// and a stage that saturates, such as an ADC, puts an end beyond its scale on
// its rail, which the range then holds too.
//
// A limit known at a node inside a path, such as the current of a 4-20 mA
// loop, is passed with the part of the path from that node on. `raw_range_of`
// is `constexpr`, so a range can be computed at compile time as the argument
// of `within`, or at run time for a validator that holds its range, e.g. from
// settings.
template<typename Path, typename Quantity>
  requires requires(Path const& path, Quantity q) {
    { path.forward(q) } -> std::totally_ordered;
  }
constexpr auto raw_range_of(Path const& path, Quantity a, Quantity b)
{
  using raw_type = decltype(path.forward(a));
  raw_type const raw_a = path.forward(a);
  raw_type const raw_b = path.forward(b);
  return raw_range<raw_type>{std::min(raw_a, raw_b), std::max(raw_a, raw_b)};
}

// The class template `within` is a validator that accepts the raw values of
// the range `Range`, its ends included, and refuses a value below `Range.min`
// with `fault::below` and a value above `Range.max` with `fault::above`. It
// takes values of the type of the range alone, so a range computed for one
// raw type does not judge the samples of another, and a range whose `min`
// exceeds its `max` does not compile. `Range` is fixed at compile time, so a
// `within` is empty and takes no storage in a `validated`; a range set at run
// time, e.g. from settings, needs a validator that holds it.
template<raw_range Range>
  requires(!(Range.max < Range.min))
struct within {
  using sample_type = decltype(Range.min);

  constexpr std::optional<fault>
  operator()(std::same_as<sample_type> auto sample) const
  {
    if (sample < Range.min) {
      return fault::below;
    }
    if (Range.max < sample) {
      return fault::above;
    }
    return std::nullopt;
  }
};

} // namespace emb::sensor
