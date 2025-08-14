#ifdef __cpp_constexpr

#include "tests.hpp"

#include <emb/generator.hpp>

#include <algorithm>
#include <array>

namespace emb {
namespace internal {
namespace tests {

constexpr bool
test_sine_generator(SineGenerator auto sine, emb::units::angle_t init_phase) {
  using output_type = decltype(sine)::output_type;

  std::array<float, 100> sine_ref;
  float time{0};
  std::generate(sine_ref.begin(), sine_ref.end(), [&]() -> float {
    float ret{time};
    time += sine.update_period();
    return ret;
  });

  std::transform(
      sine_ref.begin(),
      sine_ref.end(),
      sine_ref.begin(),
      [&](float t) -> float {
        float const w = 2 * emb::numbers::pi * sine.freq();
        float const phase = w * t + init_phase.rad().numval();
        return output_type{sine.ampl() * emb::sinf(phase)};
      });

  for (auto i{0uz}; i < sine_ref.size(); ++i) {
    EMB_CONSTEXPR_ASSERT(fabs(sine.output() - sine_ref[i]) < 0.1f);
    sine.update();
  }

  return true;
}

static_assert(test_sine_generator(
    emb::sine_generator<float>{0.1f, 1, 1},
    emb::units::angle_t{}));

static_assert(test_sine_generator(
    emb::sine_generator<float>{
        0.125f,
        100,
        1,
        emb::units::angle_t{emb::units::deg_t{90}}},
    emb::units::angle_t{emb::units::deg_t{90}}));

} // namespace tests
} // namespace internal
} // namespace emb

#endif
