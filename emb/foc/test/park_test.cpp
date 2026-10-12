#include <emb/foc/park.hpp>

#include <emb/math.hpp>
#include <emb/units.hpp>

#include <cassert>
#include <concepts>
#include <numbers>

namespace {

using emb::foc::current_ab;
using emb::foc::current_dq;
using emb::foc::invpark_transform;
using emb::foc::park_transform;
using emb::foc::sin_cos;
using emb::foc::to_sin_cos;
using emb::foc::voltage_ab;
using emb::foc::voltage_dq;
using emb::units::erad_f32;

constexpr float eps = 1e-6f;

constexpr bool near(current_dq v, float d, float q, float tol = eps)
{
  return emb::approx(v.d, d, tol) && emb::approx(v.q, q, tol);
}

constexpr bool near(current_ab v, float alpha, float beta, float tol = eps)
{
  return emb::approx(v.alpha, alpha, tol) && emb::approx(v.beta, beta, tol);
}

// both transforms keep the quantity the vector carries
static_assert(std::same_as<decltype(park_transform(voltage_ab{}, sin_cos{})),
                           voltage_dq>);
static_assert(std::same_as<decltype(invpark_transform(voltage_dq{}, sin_cos{})),
                           voltage_ab>);

constexpr bool test_axes()
{
  // a default sin_cos is the angle zero: the frames coincide
  assert(near(park_transform(current_ab{1.f, 2.f}, sin_cos{}), 1.f, 2.f));
  assert(near(invpark_transform(current_dq{1.f, 2.f}, sin_cos{}), 1.f, 2.f));

  // a quarter turn ahead, the d axis lies on beta and the q axis on -alpha
  [[maybe_unused]] constexpr sin_cos quarter{.sine = 1.f, .cosine = 0.f};
  assert(near(park_transform(current_ab{1.f, 2.f}, quarter), 2.f, -1.f));
  assert(near(invpark_transform(current_dq{2.f, -1.f}, quarter), 1.f, 2.f));

  // the inverse transform undoes the transform
  [[maybe_unused]] constexpr sin_cos theta{.sine = 0.6f, .cosine = 0.8f};
  assert(near(
      invpark_transform(park_transform(current_ab{0.5f, -0.25f}, theta), theta),
      0.5f,
      -0.25f));

  return true;
}

static_assert(test_axes());

constexpr bool test_to_sin_cos()
{
  // in constant evaluation emb::sin and emb::cos err by up to 5e-7
  [[maybe_unused]] constexpr float tol = 2e-6f;

  // a unit vector at theta lies on the d axis
  [[maybe_unused]] constexpr sin_cos theta =
      to_sin_cos(erad_f32{std::numbers::pi_v<float> / 6});
  assert(near(
      park_transform(current_ab{std::numbers::sqrt3_v<float> / 2, 0.5f}, theta),
      1.f,
      0.f,
      tol));

  // the angle zero is the default sin_cos
  [[maybe_unused]] constexpr sin_cos zero = to_sin_cos(erad_f32{0.f});
  assert(zero.sine == sin_cos{}.sine && zero.cosine == sin_cos{}.cosine);

  return true;
}

static_assert(test_to_sin_cos());

} // namespace
