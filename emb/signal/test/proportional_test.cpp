#include <emb/signal/proportional.hpp>

#include <emb/math.hpp>

namespace {

using namespace emb::units;
using emb::signal::proportional;

constexpr bool test_proportional()
{
  // a current transducer with a voltage output: +/-4 V at +/-600 A
  [[maybe_unused]] proportional const volts{amp_f32{600.f}, volt_f32{4.f}};

  assert(emb::approx(volts.forward(amp_f32{600.f}),
                     volt_f32{4.f},
                     volt_f32{1e-4f}));
  assert(emb::approx(volts.forward(amp_f32{-600.f}),
                     volt_f32{-4.f},
                     volt_f32{1e-4f}));
  assert(emb::approx(volts.forward(amp_f32{0.f}),
                     volt_f32{0.f},
                     volt_f32{1e-4f}));
  assert(emb::approx(volts.inverse(volt_f32{2.f}),
                     amp_f32{300.f},
                     amp_f32{1e-2f}));

  // beyond the rated point the line simply continues
  assert(emb::approx(volts.inverse(volts.forward(amp_f32{900.f})),
                     amp_f32{900.f},
                     amp_f32{1e-2f}));
  assert(emb::approx(volts.inverse(volts.forward(amp_f32{-37.5f})),
                     amp_f32{-37.5f},
                     amp_f32{1e-2f}));

  // in and out may be the same quantity: a closed-loop transducer whose
  // secondary carries a fixed fraction of the primary
  [[maybe_unused]] proportional const amps{amp_f32{500.f}, amp_f32{0.1f}};

  assert(emb::approx(amps.forward(amp_f32{500.f}),
                     amp_f32{0.1f},
                     amp_f32{1e-6f}));
  assert(emb::approx(amps.forward(amp_f32{-250.f}),
                     amp_f32{-0.05f},
                     amp_f32{1e-6f}));

  return true;
}

static_assert(test_proportional());

} // namespace
