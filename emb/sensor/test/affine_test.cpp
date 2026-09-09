#include <emb/sensor/affine.hpp>

#include <emb/math.hpp>

namespace {

using namespace emb::units;
using emb::sensor::affine;

constexpr bool test_affine()
{
  // a range that does not start at zero: the intercept is found by
  // subtraction, which is where this form loses accuracy if it ever does
  [[maybe_unused]] affine const hot{degree_celsius_f32{100.f},
                   degree_celsius_f32{200.f},
                   volt_f32{1.f},
                   volt_f32{5.f}};
  assert(emb::approx(hot.forward(degree_celsius_f32{100.f}),
                     volt_f32{1.f},
                     volt_f32{1e-5f}));
  assert(emb::approx(hot.forward(degree_celsius_f32{200.f}),
                     volt_f32{5.f},
                     volt_f32{1e-5f}));
  assert(emb::approx(hot.forward(degree_celsius_f32{150.f}),
                     volt_f32{3.f},
                     volt_f32{1e-5f}));
  assert(emb::approx(hot.inverse(volt_f32{3.f}),
                     degree_celsius_f32{150.f},
                     degree_celsius_f32{1e-3f}));

  // an output range that falls as the input rises
  [[maybe_unused]] affine const falling{megapascal_f32{0.f},
                       megapascal_f32{2.5f},
                       amp_f32{0.020f},
                       amp_f32{0.004f}};
  assert(emb::approx(falling.forward(megapascal_f32{0.f}),
                     amp_f32{0.020f},
                     amp_f32{1e-7f}));
  assert(emb::approx(falling.forward(megapascal_f32{2.5f}),
                     amp_f32{0.004f},
                     amp_f32{1e-7f}));

  // outside the input range it extrapolates rather than clamping
  assert(emb::approx(hot.forward(degree_celsius_f32{300.f}),
                     volt_f32{9.f},
                     volt_f32{1e-5f}));

  return true;
}

static_assert(test_affine());

} // namespace
