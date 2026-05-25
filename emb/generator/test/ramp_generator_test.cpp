#include <emb/generator/ramp_generator.hpp>
#include <emb/units.hpp>

#include <algorithm>
#include <cstddef>

namespace {

template<typename Ramp>
constexpr bool test_ramp_generator(
    Ramp ramp,
    typename Ramp::value_type target,
    typename Ramp::value_type slope,
    emb::units::sec_f32 timestep
) {
  using value_type = typename Ramp::value_type;

  value_type const init_output{ramp.output()};
  assert(ramp.at_target());
  assert(ramp.target() == init_output);

  ramp.set_target(target);
  assert(ramp.target() == target);
  assert(ramp.at_target() == (init_output == target));

  value_type const step = slope * timestep.value();
  value_type const dir = target > init_output ? value_type{1} : value_type{-1};

  constexpr std::size_t max_iterations = 200;
  for (auto i = 0uz; i < max_iterations; ++i) {
    value_type const unclamped = init_output
                               + dir * step * static_cast<value_type>(i);
    [[maybe_unused]] value_type const expected = dir > value_type{0}
                                                   ? std::min(unclamped, target)
                                                   : std::max(
                                                         unclamped,
                                                         target
                                                     );
    assert(fabs(ramp.output() - expected) < 1e-4f);
    if (ramp.at_target()) break;
    ramp.update();
  }

  assert(ramp.at_target());
  assert(ramp.output() == target);

  ramp.update();
  assert(ramp.output() == target);
  assert(ramp.at_target());

  return true;
}

static_assert(test_ramp_generator(
    emb::ramp_generator<float>{emb::units::sec_f32{0.1f}, 1.0f},
    1.0f,
    1.0f,
    emb::units::sec_f32{0.1f}
));

static_assert(test_ramp_generator(
    emb::ramp_generator<float>{emb::units::sec_f32{0.01f}, 50.0f, -10.0f},
    10.0f,
    50.0f,
    emb::units::sec_f32{0.01f}
));

static_assert(test_ramp_generator(
    emb::ramp_generator<float>{emb::units::sec_f32{0.1f}, 100.0f, 100.0f},
    0.0f,
    100.0f,
    emb::units::sec_f32{0.1f}
));

static_assert(test_ramp_generator(
    emb::ramp_generator<float>{emb::units::sec_f32{0.05f}, 5.0f, 2.0f},
    2.0f,
    5.0f,
    emb::units::sec_f32{0.05f}
));

constexpr bool test_ramp_reset() {
  emb::ramp_generator<float> ramp(emb::units::sec_f32{0.1f}, 10.0f, 5.0f);
  assert(ramp.output() == 5.0f);
  assert(ramp.target() == 5.0f);
  assert(ramp.at_target());

  ramp.set_target(10.0f);
  while (!ramp.at_target()) {
    ramp.update();
  }
  assert(ramp.output() == 10.0f);

  ramp.reset();
  assert(ramp.output() == 5.0f);
  assert(ramp.target() == 5.0f);
  assert(ramp.at_target());

  return true;
}

constexpr bool test_ramp_set_output() {
  emb::ramp_generator<float> ramp(emb::units::sec_f32{0.1f}, 1.0f, 0.0f);

  ramp.set_target(10.0f);
  ramp.update();
  ramp.update();
  assert(!ramp.at_target());

  ramp.set_output(3.5f);
  assert(ramp.output() == 3.5f);
  assert(ramp.target() == 3.5f);
  assert(ramp.at_target());

  return true;
}

constexpr bool test_ramp_set_timestep() {
  emb::ramp_generator<float> ramp(emb::units::sec_f32{0.1f}, 1.0f, 0.0f);
  ramp.set_target(1.0f);

  ramp.update();
  assert(fabs(ramp.output() - 0.1f) < 1e-4f);

  ramp.set_timestep(emb::units::sec_f32{0.2f});
  ramp.update();
  assert(fabs(ramp.output() - 0.3f) < 1e-4f);

  return true;
}

constexpr bool test_ramp_set_slope() {
  emb::ramp_generator<float> ramp(emb::units::sec_f32{0.1f}, 1.0f, 0.0f);
  ramp.set_target(1.0f);

  ramp.update();
  assert(fabs(ramp.output() - 0.1f) < 1e-4f);

  ramp.set_slope(emb::units::sec_f32{0.1f}, 2.0f);
  ramp.update();
  assert(fabs(ramp.output() - 0.3f) < 1e-4f);

  return true;
}

constexpr bool test_ramp_change_target_midway() {
  emb::ramp_generator<float> ramp(emb::units::sec_f32{0.1f}, 1.0f, 0.0f);

  ramp.set_target(10.0f);
  for (int i{0}; i < 5; ++i) {
    ramp.update();
  }
  assert(fabs(ramp.output() - 0.5f) < 1e-4f);

  ramp.set_target(0.0f);
  while (!ramp.at_target()) {
    ramp.update();
  }
  assert(ramp.output() == 0.0f);

  return true;
}

static_assert(test_ramp_reset());
static_assert(test_ramp_set_output());
static_assert(test_ramp_set_timestep());
static_assert(test_ramp_set_slope());
static_assert(test_ramp_change_target_midway());

} // namespace
