#ifdef __cpp_constexpr

#include "tests.hpp"

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
  EMB_CONSTEXPR_ASSERT(convert_to<deg_f32>(deg1) == deg1);
  EMB_CONSTEXPR_ASSERT(convert_to<rad_f32>(deg1) == rad_f32{emb::to_rad(v1)});

  EMB_CONSTEXPR_ASSERT(emb::rem360(deg_f32{380.0f}) == deg_f32(20.0f));
  EMB_CONSTEXPR_ASSERT(emb::rem360(deg_f32{-30.0f}) == deg_f32(330.0f));

  EMB_CONSTEXPR_ASSERT(emb::rem180(deg_f32{200.0f}) == deg_f32(-160.0f));
  EMB_CONSTEXPR_ASSERT(emb::rem180(deg_f32{-10.0f}) == deg_f32(-10.0f));

  float v2{3000.0f};
  int32_t p{4};
  rpm_f32 rpm1{v2};
  EMB_CONSTEXPR_ASSERT(convert_to<rpm_f32>(rpm1) == rpm1);
  EMB_CONSTEXPR_ASSERT(
      convert_to<eradps_f32>(rpm1, p) == eradps_f32{emb::to_eradps(v2, p)});

  EMB_CONSTEXPR_ASSERT(
      templated_conversion<erad_f32>() == erad_f32{emb::numbers::pi});

  EMB_CONSTEXPR_ASSERT(
      templated_conversion<edeg_f32>() ==
      edeg_f32{emb::to_deg(emb::numbers::pi)});

  return true;
}

static_assert(test_units_conversion());

} // namespace tests
} // namespace internal
} // namespace emb

#endif
