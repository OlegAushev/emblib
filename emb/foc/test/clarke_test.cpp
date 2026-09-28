#include <emb/foc/clarke.hpp>

#include <emb/math.hpp>

#include <cassert>
#include <numbers>

namespace {

using emb::foc::clarke_inputs;
using emb::foc::clarke_transform;
using emb::foc::current_abc;
using emb::foc::current_ab;
using emb::foc::invclarke_transform;

constexpr float eps = 1e-6f;

constexpr bool near(current_ab v, float alpha, float beta)
{
  return emb::approx(v.alpha, alpha, eps) && emb::approx(v.beta, beta, eps);
}

constexpr bool near(current_abc v, float a, float b, float c)
{
  return emb::approx(v.a, a, eps)
      && emb::approx(v.b, b, eps)
      && emb::approx(v.c, c, eps);
}

template<clarke_inputs N>
constexpr bool test_balanced()
{
  [[maybe_unused]] constexpr float h = std::numbers::sqrt3_v<float> / 2;

  // unit amplitude at 0, 30 and 90 degrees: both forms give the same vector
  assert(near(clarke_transform<N>(current_abc{1.f, -0.5f, -0.5f}), 1.f, 0.f));
  assert(near(clarke_transform<N>(current_abc{h, 0.f, -h}), h, 0.5f));
  assert(near(clarke_transform<N>(current_abc{0.f, h, -h}), 0.f, 1.f));

  return true;
}

static_assert(test_balanced<clarke_inputs::two>());
static_assert(test_balanced<clarke_inputs::three>());

constexpr bool test_zero_sequence()
{
  // a common offset in every phase: three discards it, two does not
  assert(
      near(clarke_transform<clarke_inputs::three>(current_abc{3.f, 0.f, 0.f}),
           2.f,
           0.f));
  assert(near(clarke_transform<clarke_inputs::two>(current_abc{3.f, 0.f, 0.f}),
              3.f,
              std::numbers::sqrt3_v<float>));
  assert(
      near(clarke_transform<clarke_inputs::three>(current_abc{5.f, 5.f, 5.f}),
           0.f,
           0.f));

  // two reads a and b only
  assert(
      near(clarke_transform<clarke_inputs::two>(current_abc{1.f, 2.f, 1e30f}),
           1.f,
           5.f * std::numbers::inv_sqrt3_v<float>));

  // the inverse returns the input less its zero sequence
  [[maybe_unused]] current_abc const unbalanced{3.f, 1.f, -1.f};
  assert(near(
      invclarke_transform(clarke_transform<clarke_inputs::three>(unbalanced)),
      2.f,
      0.f,
      -2.f));

  return true;
}

static_assert(test_zero_sequence());

} // namespace
