#pragma once

#include <emb/units/chrono.hpp>

#include <chrono>
#include <concepts>
#include <cstdint>

namespace emb::chrono {
// TODO
template<typename Rep, std::intmax_t ClockHz, std::intmax_t Divider = 1>
class timebase {
  static_assert(ClockHz > 0 && Divider > 0);
public:
  using duration = std::chrono::duration<Rep, std::ratio<Divider, ClockHz>>;

  static constexpr duration ticks(Rep r)
  {
    return duration{r};
  }
};

} // namespace emb::chrono
