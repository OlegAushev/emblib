#pragma once

#include <emb/container/circular_buffer.hpp>
#include <emb/filter/exponential_median_filter.hpp>
#include <emb/gpio.hpp>
#include <emb/hall/calibration.hpp>
#include <emb/hall/error.hpp>
#include <emb/hall/sector.hpp>
#include <emb/units.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <expected>
#include <optional>

namespace emb::hall {

template<typename T>
concept some_timebase = requires(T const t) {
  { t.period() } -> std::same_as<emb::units::sec_f32>;
};

template<typename T>
concept some_capture_timer = requires(T const t) {
  { t.input_state() } -> std::convertible_to<std::uint8_t>;
  { t.input_levels() } -> std::convertible_to<std::array<emb::gpio::level, 3>>;
  { t.captured_time() } -> std::same_as<emb::units::sec_f32>;
  { t.time_since_capture() } -> std::same_as<emb::units::sec_f32>;
};

// -------------------------------------------------------- Angle Sensor Config

struct angle_sensor_config {
  emb::units::sec_f32 speed_timeconstant;
  calibration_result cal_result;
};

constexpr std::expected<void, error> validate(angle_sensor_config const& conf)
{
  if (auto const calibration = validate(conf.cal_result); !calibration) {
    return calibration;
  }
  if (conf.speed_timeconstant.value() <= 0) {
    return std::unexpected(error::invalid_config);
  }
  return {};
}

// --------------------------------------------------------------- Angle Sensor

template<some_capture_timer Timer>
class angle_sensor {
private:
  using speed_filter_type =
      emb::exponential_median_filter<emb::units::eradps_f32,
                                     3,
                                     emb::units::sec_f32>;

  angle_sensor_config conf_;

  emb::units::sec_f32 timestep_;
  Timer& capture_timer_;
  std::optional<emb::units::sec_f32> new_transition_period_ = std::nullopt;

  sector_geometry geometry_{};

  // sectors recognized in the last two pollings
  emb::circular_buffer<sector, 2> sector_history_{};

  speed_filter_type speedfilter_;

  sector_span sector_span_{};
  emb::units::erad_f32 offset_{};
  emb::units::erad_f32 angle_{};
public:
  angle_sensor(some_timebase auto const& timebase,
               Timer& capture_timer,
               angle_sensor_config const& sensor_conf)
      : conf_(sensor_conf),
        timestep_(timebase.period()),
        capture_timer_(capture_timer),
        speedfilter_(timestep_, sensor_conf.speed_timeconstant)
  {
    geometry_ = make_geometry(sensor_conf.cal_result);

    sector_history_.push_back(capture_sector().value_or(sector::a));
    sector_span_ = geometry_.fwd[sector_history_.back()];
    angle_ = sector_span_.entry + sector_halfwidth;
  }

  void set_timestep(emb::units::sec_f32 ts)
  {
    timestep_ = ts;
  }

  void on_transition()
  {
    new_transition_period_ = capture_timer_.captured_time();
  }

  [[nodiscard]] std::optional<error> update()
  {
    if (new_transition_period_) {
      return on_new_transition();
    }

    offset_ += speedfilter_.output() * timestep_;
    if (offset_.value() >= 0) {
      offset_ = std::min(offset_, sector_span_.width);
    }
    else {
      offset_ = std::max(offset_, -sector_span_.width);
    }
    angle_ = sector_span_.entry + offset_;
    return {};
  }

  void on_timeout()
  {
    offset_ = emb::units::erad_f32{0};
    angle_ = sector_span_.entry + sector_halfwidth;
    speedfilter_.set_output(emb::units::eradps_f32{0});
  }

  emb::units::erad_f32 angle() const
  {
    return emb::norm2pi_fast(angle_);
  }

  emb::units::eradps_f32 speed() const
  {
    return speedfilter_.output();
  };

  sector current_sector() const
  {
    return sector_history_.back();
  }

  std::optional<sector> capture_sector() const
  {
    return decode_sector(capture_timer_.input_state());
  }

  emb::gpio::level input_a() const
  {
    return capture_timer_.input_levels()[0];
  }

  emb::gpio::level input_b() const
  {
    return capture_timer_.input_levels()[1];
  }

  emb::gpio::level input_c() const
  {
    return capture_timer_.input_levels()[2];
  }

  angle_sensor_config const& config() const
  {
    return conf_;
  }

  [[nodiscard]] std::expected<void, error>
  configure(angle_sensor_config const& conf)
  {
    conf_ = conf;
    speedfilter_.set_smoothing(timestep_, conf.speed_timeconstant);
    geometry_ = make_geometry(conf.cal_result);
    return validate(conf);
  }

  [[nodiscard]] std::expected<void, error>
  apply_calibration(calibration_result const& data)
  {
    if (auto const checked = validate(data); !checked) {
      return checked;
    }
    conf_.cal_result = data;
    geometry_ = make_geometry(data);
    return {};
  }
private:
  std::optional<error> on_new_transition()
  {
    auto const transition_period = *new_transition_period_;
    new_transition_period_.reset();

    auto const time_since_transition = capture_timer_.time_since_capture();
    auto const s = capture_sector();
    if (!s.has_value()) {
      return error::invalid_input;
    }

    sector_history_.push_back(*s);

    auto const sector_to = sector_history_.back();
    auto const sector_from = sector_history_.front();

    if (sector_to == sector_from) {
      return std::nullopt;
    }

    float const sign = geometry_.signs.at(sector_to, sector_from);
    bool const is_forward = sign >= 0.0f;
    auto const& sectors = is_forward ? geometry_.fwd : geometry_.rev;

    auto const raw_speed =
        sectors[sector_from].width / transition_period * sign;
    speedfilter_.set_timestep(transition_period);
    speedfilter_.push(raw_speed);

    sector_span_ = sectors[sector_to];
    offset_ = speedfilter_.output() * time_since_transition;
    angle_ = sector_span_.entry + offset_;

    return std::nullopt;
  }
};

} // namespace emb::hall
