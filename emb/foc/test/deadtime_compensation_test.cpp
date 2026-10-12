#include <emb/foc/deadtime_compensation.hpp>

#include <emb/math.hpp>

namespace {

using emb::unsigned_pu_f32;
using emb::foc::compensate_deadtime;
using emb::foc::current_abc;

using dutycycles = emb::three_phase<unsigned_pu_f32>;

constexpr float eps = 1e-6f;

constexpr dutycycles make(float a, float b, float c)
{
  return {.a = unsigned_pu_f32{a},
          .b = unsigned_pu_f32{b},
          .c = unsigned_pu_f32{c}};
}

constexpr bool near(dutycycles d, float a, float b, float c)
{
  return emb::approx(d.a.value(), a, eps)
      && emb::approx(d.b.value(), b, eps)
      && emb::approx(d.c.value(), c, eps);
}

constexpr unsigned_pu_f32 dt{0.01f};

// at and above the threshold: the full correction, along the current
static_assert(
    near(compensate_deadtime(make(0.5f, 0.5f, 0.5f),
                             current_abc{.a = 2.f, .b = -1.f, .c = -1.f},
                             dt,
                             1.f),
         0.51f,
         0.49f,
         0.49f));

// below the threshold: proportional to the current
static_assert(
    near(compensate_deadtime(make(0.5f, 0.5f, 0.5f),
                             current_abc{.a = 0.5f, .b = -0.25f, .c = -0.25f},
                             dt,
                             1.f),
         0.505f,
         0.4975f,
         0.4975f));

// a zero threshold: the full correction to any nonzero current, none to zero
static_assert(
    near(compensate_deadtime(make(0.5f, 0.5f, 0.5f),
                             current_abc{.a = 0.f, .b = 1e-3f, .c = -1e-3f},
                             dt,
                             0.f),
         0.5f,
         0.51f,
         0.49f));

// clamped to [0, 1]
static_assert(
    near(compensate_deadtime(make(1.f, 0.f, 0.995f),
                             current_abc{.a = 1.f, .b = -2.f, .c = 1.f},
                             dt,
                             0.5f),
         1.f,
         0.f,
         1.f));

} // namespace
