#ifdef __cpp_constexpr

#include <emb/units.hpp>

namespace emb {
namespace internal {
namespace tests {

template<typename Unit>
constexpr Unit templated_conversion() {
  return emb::units::convert_to<Unit>(emb::units::erad_f32{emb::numbers::pi});
}

constexpr bool test_units_conversion() {
  using namespace emb::units;

  float v1{90.0f};
  deg_f32 deg1{v1};
  assert(convert_to<deg_f32>(deg1) == deg1);
  assert(convert_to<rad_f32>(deg1) == rad_f32{emb::to_rad(v1)});

  assert(emb::rem360(deg_f32{380.0f}) == deg_f32(20.0f));
  assert(emb::rem360(deg_f32{-30.0f}) == deg_f32(330.0f));

  assert(emb::rem180(deg_f32{200.0f}) == deg_f32(-160.0f));
  assert(emb::rem180(deg_f32{-10.0f}) == deg_f32(-10.0f));

  float v2{3000.0f};
  [[maybe_unused]] int32_t p{4};
  rpm_f32 rpm1{v2};
  assert(convert_to<rpm_f32>(rpm1) == rpm1);
  assert(convert_to<eradps_f32>(rpm1, p) == eradps_f32{emb::to_eradps(v2, p)});

  assert(templated_conversion<erad_f32>() == erad_f32{emb::numbers::pi});

  assert(
      templated_conversion<edeg_f32>() ==
      edeg_f32{emb::to_deg(emb::numbers::pi)}
  );

  [[maybe_unused]] hz_f32 freq{100};
  assert(1 / freq == sec_f32{1.0f / 100.0f});

  [[maybe_unused]] sec_f32 per{0.0001f};
  assert(1 / per == hz_f32(1 / 0.0001f));

  return true;
}

static_assert(test_units_conversion());

} // namespace tests
} // namespace internal
} // namespace emb

#endif
