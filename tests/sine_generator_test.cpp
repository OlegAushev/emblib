#ifdef __cpp_constexpr

#include "tests.hpp"

#include <emb/generator.hpp>

#include <algorithm>
#include <array>

namespace emb {
namespace internal {
namespace tests {

constexpr bool
test_sine_generator(SineGenerator auto sine,
                    emb::units::angle_t init_phase) {
  using output_type = decltype(sine)::output_type;

  std::array<float, 100> timebase;
  std::array<output_type, 100> sine_ref;
  float time{0};
  std::generate(timebase.begin(), timebase.end(), [&]() -> float {
    float ret{time};
    time += sine.update_period();
    return ret;
  });

  std::transform(
      timebase.begin(),
      timebase.end(),
      sine_ref.begin(),
      [&](float t) -> output_type {
        float const w = 2 * emb::numbers::pi * sine.freq();
        float const phase = w * t + init_phase.rad().numval();
        return sine.ampl() * emb::sin(phase) + sine.bias();
      });

  for (auto i{0uz}; i < sine_ref.size(); ++i) {
    EMB_CONSTEXPR_ASSERT(
        fabs(sine.output() - sine_ref[i]) < 0.001f * sine.ampl());
    sine.update();
  }

  return true;
}

static_assert(test_sine_generator(
    emb::sine_generator<float>{0.1f, 1.0f, 1.0f},
    emb::units::angle_t{}));

static_assert(test_sine_generator(
    emb::sine_generator<float>{
        0.125f,
        100.0f,
        1.0f,
        emb::units::angle_t{emb::units::deg_f32{90.0f}}},
    emb::units::angle_t{emb::units::deg_f32{90.0f}}));

static_assert(test_sine_generator(
    emb::sine_generator<float>{
        0.1f,
        1.0f,
        4.2f,
        emb::units::angle_t{emb::units::deg_f32{42.0f}},
        42.0f},
    emb::units::angle_t{emb::units::deg_f32{42.0f}}));

} // namespace tests
} // namespace internal
} // namespace emb

#endif
