#include <emb/math/scaled.hpp>

#include <emb/math/clamped.hpp>
#include <emb/units.hpp>

#include <cassert>
#include <cstdint>
#include <ratio>
#include <type_traits>

namespace {

using volt_ds =
    emb::scaled<std::int16_t, emb::units::volt_f32, std::ratio<1, 10>>;
using torque_pu =
    emb::scaled<std::int16_t, emb::signed_pu_f32, std::ratio<1, 10000>>;

// a layout built from these must still bit_cast to its wire image whole
static_assert(sizeof(volt_ds) == sizeof(std::int16_t));
static_assert(alignof(volt_ds) == alignof(std::int16_t));
static_assert(std::is_trivially_copyable_v<volt_ds>);
static_assert(std::has_unique_object_representations_v<volt_ds>);

// one step, one type, however the ratio is spelled
static_assert(
    std::is_same_v<
        volt_ds,
        emb::scaled<std::int16_t, emb::units::volt_f32, std::ratio<3, 30>>>);

// both wrapper families qualify; a bare float does not
static_assert(emb::some_quantity<emb::units::volt_f32>);
static_assert(emb::some_quantity<emb::signed_pu_f32>);
static_assert(!emb::some_quantity<float>);
static_assert(!emb::some_quantity<int>);

static_assert(emb::some_ratio<std::ratio<1, 10>>);
static_assert(!emb::some_ratio<int>);

constexpr bool test_scaled()
{
  [[maybe_unused]] constexpr auto near = [](float a, float b) {
    return (a - b) < 1e-4f && (b - a) < 1e-4f;
  };

  // saturate_round
  assert(emb::saturate_round<std::int16_t>(1.5f) == 2);
  assert(emb::saturate_round<std::int16_t>(-1.5f) == -2);
  assert(emb::saturate_round<std::int16_t>(1.4f) == 1);
  assert(emb::saturate_round<std::int16_t>(1e9f) == 32767);
  assert(emb::saturate_round<std::int16_t>(-1e9f) == -32768);

  // quantize: a count is the step, and out of range saturates
  assert((emb::quantize<std::int16_t, std::ratio<1, 10000>>(1.0f)) == 10000);
  assert((emb::quantize<std::int16_t, std::ratio<1, 10000>>(-1.0f)) == -10000);
  assert((emb::quantize<std::int16_t, std::ratio<1, 10>>(5000.0f)) == 32767);

  // dequantize: exact where the step is exact
  assert((emb::dequantize<std::ratio<1, 2>>(5)) == 2.5f);
  assert(near(emb::dequantize<std::ratio<1, 10>>(241), 24.1f));

  // a default field reads zero
  constexpr volt_ds zero{};
  assert(zero.raw() == 0);
  assert(zero.value() == emb::units::volt_f32{0});

  // assignment encodes, value() decodes
  volt_ds v{};
  v = emb::units::volt_f32{24.1f};
  assert(v.raw() == 241);
  assert(near(v.value().value(), 24.1f));

  // beyond the raw range saturates rather than wraps
  v = emb::units::volt_f32{5000.0f};
  assert(v.raw() == 32767);

  torque_pu t{emb::signed_pu_f32{0.5f}};
  assert(t.raw() == 5000);
  assert(near(t.value().value(), 0.5f));

  // the quantity's own invariant holds before quantization
  t = emb::signed_pu_f32{2.0f};
  assert(t.raw() == 10000);

  return true;
}

static_assert(test_scaled());

} // namespace
