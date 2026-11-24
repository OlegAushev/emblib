#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace emb {

template<std::unsigned_integral Accumulator, std::floating_point Phase>
class phase_accumulator {
  static_assert(!std::is_same_v<Accumulator, bool>);
public:
  using accumulator_type = Accumulator;
  using phase_type = Phase;

  constexpr phase_accumulator() = default;

  constexpr phase_accumulator(accumulator_type initial_ticks)
      : acc_(initial_ticks)
  {
  }

  constexpr void advance(accumulator_type count)
  {
    acc_ += count;
  }

  constexpr phase_type phase() const
  {
    constexpr int w = std::numeric_limits<Accumulator>::digits;
    constexpr int p = std::numeric_limits<Phase>::digits;
    constexpr int b = std::min(w, p);
    constexpr Phase scale = std::ldexp(Phase{1}, -b);

    using narrow = std::conditional_t<
        (b <= 32),
        std::uint32_t,
        std::conditional_t<(b <= 64), std::uint64_t, Accumulator>>;

    auto const ticks = static_cast<narrow>(acc_ >> (w - b));

    return static_cast<Phase>(ticks) * scale;
  }

  constexpr void reset(accumulator_type ticks)
  {
    acc_ = ticks;
  }
private:
  Accumulator acc_{};
};

} // namespace emb
