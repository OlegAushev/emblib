#include <emb/hall/angle_sensor.hpp>

#include <expected>
#include <limits>

namespace {

using namespace emb::hall;
using sec = emb::units::sec_f32;

// Two configs are equal if their time constants are.
static_assert(angle_sensor_config{sec{0.002f}}
              == angle_sensor_config{sec{0.002f}});
static_assert(angle_sensor_config{sec{0.002f}}
              != angle_sensor_config{sec{0.004f}});

// The time constant of the speed filter is positive, and NaN is not.
static_assert(validate(angle_sensor_config{sec{0.002f}}).has_value());
static_assert(validate(angle_sensor_config{sec{0.0f}})
              == std::unexpected(error::invalid_config));
static_assert(!validate(angle_sensor_config{
    sec{std::numeric_limits<float>::quiet_NaN()}}));

} // namespace
