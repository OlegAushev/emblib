#include <emb/generator/sine_generator.hpp>
#include <emb/units.hpp>

#include <algorithm>
#include <array>

namespace {

template<typename G>
constexpr bool test_sine_generator(G sine, emb::units::rad_f32 init_phase) {
  using output_type = decltype(sine)::output_type;

  std::array<emb::units::sec_f32, 100> timebase;
  std::array<output_type, 100> sine_ref;
  emb::units::sec_f32 time{0};
  std::generate(timebase.begin(), timebase.end(), [&]() -> emb::units::sec_f32 {
    emb::units::sec_f32 ret{time};
    time += sine.update_period();
    return ret;
  });

  std::transform(
      timebase.begin(),
      timebase.end(),
      sine_ref.begin(),
      [&](emb::units::sec_f32 t) -> output_type {
        float const w = 2 * std::numbers::pi_v<float> * sine.freq();
        float const phase = w * t.value() + init_phase.value();
        return sine.ampl() * emb::sin(phase) + sine.bias();
      }
  );

  for (auto i{0uz}; i < sine_ref.size(); ++i) {
    assert(fabs(sine.output() - sine_ref[i]) < 0.001f * sine.ampl());
    sine.update();
  }

  return true;
}

static_assert(test_sine_generator(
    emb::sine_generator<float>{
        emb::units::sec_f32{0.1f},
        1.0f,
        emb::units::hz_f32{1.0f}
    },
    emb::units::rad_f32{}
));

static_assert(test_sine_generator(
    emb::sine_generator<float>{
        emb::units::sec_f32{0.125f},
        100.0f,
        emb::units::hz_f32{1.0f},
        emb::units::convert_to<emb::units::rad_f32>(emb::units::deg_f32{90.0f})
    },
    emb::units::convert_to<emb::units::rad_f32>(emb::units::deg_f32{90.0f})
));

static_assert(test_sine_generator(
    emb::sine_generator<float>{
        emb::units::sec_f32{0.1f},
        1.0f,
        emb::units::hz_f32{4.2f},
        emb::units::convert_to<emb::units::rad_f32>(emb::units::deg_f32{42.0f}),
        42.0f
    },
    emb::units::convert_to<emb::units::rad_f32>(emb::units::deg_f32{42.0f})
));

} // namespace
