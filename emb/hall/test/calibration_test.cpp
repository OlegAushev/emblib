#include <emb/hall/calibration.hpp>

#include <limits>

namespace {

using namespace emb::hall;
using deg = emb::units::edeg_f32;

inline constexpr sector_map<deg> fwd{{deg{330.0f},
                                      deg{30.0f},
                                      deg{90.0f},
                                      deg{150.0f},
                                      deg{210.0f},
                                      deg{270.0f}}};

inline constexpr sector_map<deg> rev{{deg{30.0f},
                                      deg{90.0f},
                                      deg{150.0f},
                                      deg{210.0f},
                                      deg{270.0f},
                                      deg{330.0f}}};

consteval bool accepts(calibration_result const& data)
{
  return validate(data).has_value();
}

consteval calibration_result with_fwd(sector s, deg v)
{
  calibration_result data{fwd, rev};
  data.fwd_sector_angles[s] = v;
  return data;
}

consteval calibration_result with_rev(sector s, deg v)
{
  calibration_result data{fwd, rev};
  data.rev_sector_angles[s] = v;
  return data;
}

consteval bool refused_as_invalid_calibration(calibration_result const& data)
{
  auto const checked = validate(data);
  return !checked && checked.error() == error::invalid_calibration;
}

static_assert(accepts({fwd, rev}));

// The range is half-open: 0 is in, 360 is out.
static_assert(accepts(with_fwd(sector::a, deg{0.0f})));
static_assert(refused_as_invalid_calibration(with_fwd(sector::a, deg{360.0f})));
static_assert(refused_as_invalid_calibration(with_rev(sector::c, deg{-30.0f})));

// Not a number, nor an infinity, is in range.
static_assert(refused_as_invalid_calibration(
    with_fwd(sector::b, deg{std::numeric_limits<float>::quiet_NaN()})));
static_assert(refused_as_invalid_calibration(
    with_rev(sector::bc, deg{std::numeric_limits<float>::infinity()})));

// Two sectors of one direction may not start at the same angle; the two
// directions are independent of each other.
static_assert(refused_as_invalid_calibration(with_fwd(sector::a, deg{30.0f})));
static_assert(refused_as_invalid_calibration(with_rev(sector::ca, deg{30.0f})));
static_assert(accepts(calibration_result{fwd, fwd}));

} // namespace
