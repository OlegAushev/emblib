#include <emb/hall/angle_sensor.hpp>

namespace {

using namespace emb::hall;
using deg = emb::units::edeg_f32;
using sec = emb::units::sec_f32;

inline constexpr calibration_result calibration{
    sector_map<deg>{{deg{330.0f},
                     deg{30.0f},
                     deg{90.0f},
                     deg{150.0f},
                     deg{210.0f},
                     deg{270.0f}}},
    sector_map<deg>{{deg{30.0f},
                     deg{90.0f},
                     deg{150.0f},
                     deg{210.0f},
                     deg{270.0f},
                     deg{330.0f}}}};

consteval calibration_result with_fwd(sector s, deg v)
{
  calibration_result data = calibration;
  data.fwd_sector_angles[s] = v;
  return data;
}

// Two configs are equal if the time constant and the calibration are.
static_assert(angle_sensor_config{sec{0.002f}, calibration}
              == angle_sensor_config{sec{0.002f}, calibration});
static_assert(angle_sensor_config{sec{0.002f}, calibration}
              != angle_sensor_config{sec{0.004f}, calibration});
static_assert(angle_sensor_config{sec{0.002f}, calibration}
              != angle_sensor_config{sec{0.002f},
                                     with_fwd(sector::b, deg{100.0f})});

} // namespace
