#pragma once

#include <emb/units/chrono.hpp>

#include <cassert>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>

namespace emb::chrono {

// The class template `timebase` represents hardware timer intervals as tick
// counts of integral type `Rep`. One tick spans `Divider` cycles of a clock
// running at `ClockHz` hertz. Both `ClockHz` and `Divider` must be positive.
template<std::integral Rep, std::intmax_t ClockHz, std::intmax_t Divider = 1>
class timebase {
  static_assert(ClockHz > 0 && Divider > 0);
public:
  using period = std::ratio<Divider, ClockHz>;
  using duration = std::chrono::duration<Rep, period>;

  static constexpr duration ticks(Rep r)
  {
    return duration{r};
  }

  template<std::floating_point T = float>
  static constexpr units::sec<T> seconds(duration d)
  {
    return units::sec<T>{std::chrono::duration<T>{d}.count()};
  }

  template<std::floating_point T = float>
  static constexpr units::hz<T> frequency(duration d)
  {
    assert(d.count() > 0);

    return units::hz<T>{
        static_cast<T>(period::den)
        / (static_cast<T>(period::num) * static_cast<T>(d.count()))};
  }

  // Converts `t` to the nearest whole number of ticks, rounding ties to even
  // as if by `std::chrono::round`. The interval must be finite and nonnegative.
  // Before rounding, the tick count computed in `T` must be strictly less
  // than `std::numeric_limits<Rep>::max()`.
  template<std::floating_point T>
  static constexpr duration from_seconds(units::sec<T> t)
  {
    assert(std::isfinite(t.value) && t.value >= T{0});

    using fractional_ticks = std::chrono::duration<T, period>;
    auto const ticks = fractional_ticks{std::chrono::duration<T>{t.value}};
    return std::chrono::round<duration>(ticks);
  }

  // Returns the period of `f` in ticks. Computes `T{1} / f.value` in `T`
  // and passes that period in seconds to `from_seconds` for rounding.
  // The frequency must be finite and positive, and the computed period must
  // satisfy the preconditions of `from_seconds`.
  template<std::floating_point T>
  static constexpr duration from_frequency(units::hz<T> f)
  {
    assert(std::isfinite(f.value) && f.value > T{0});

    return from_seconds(units::sec<T>{T{1} / f.value});
  }
};

} // namespace emb::chrono
