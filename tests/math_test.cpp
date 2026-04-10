#ifdef __cpp_constexpr

#include <emb/math.hpp>

namespace emb {
namespace internal {
namespace tests {

constexpr bool test_math() {
  constexpr auto near = [](float a, float b) {
    return (a - b) < 1e-4f && (b - a) < 1e-4f;
  };

  // sgn
  assert(emb::sgn(10) == 1);
  assert(emb::sgn(-5) == -1);
  assert(emb::sgn(0) == 0);

  // iseven / isodd
  assert(emb::iseven(0));
  assert(emb::iseven(4));
  assert(!emb::iseven(3));
  assert(emb::isodd(1));
  assert(emb::isodd(-3));
  assert(!emb::isodd(2));

  // to_rad / to_deg
  assert(near(emb::to_rad(180.0f), std::numbers::pi_v<float>));
  assert(near(emb::to_rad(90.0f), std::numbers::pi_v<float> / 2));
  assert(near(emb::to_deg(std::numbers::pi_v<float>), 180.0f));
  assert(near(emb::to_deg(std::numbers::pi_v<float> / 2), 90.0f));

  // to_eradps / to_rpm
  assert(near(emb::to_eradps(60.0f, 1), 2 * std::numbers::pi_v<float>));
  assert(near(emb::to_rpm(2 * std::numbers::pi_v<float>, 1), 60.0f));

  // rem2pi
  constexpr float pi = std::numbers::pi_v<float>;
  constexpr float two_pi = 2 * pi;
  assert(near(emb::rem2pi(0.0f), 0.0f));
  assert(near(emb::rem2pi(two_pi + 1.0f), 1.0f));
  assert(near(emb::rem2pi(-1.0f), two_pi - 1.0f));

  // rempi
  assert(near(emb::rempi(0.0f), 0.0f));
  assert(near(emb::rempi(pi + 0.5f), -pi + 0.5f));
  assert(near(emb::rempi(-pi + 0.5f), -pi + 0.5f));

  return true;
}

static_assert(test_math());

} // namespace tests
} // namespace internal
} // namespace emb

#endif
