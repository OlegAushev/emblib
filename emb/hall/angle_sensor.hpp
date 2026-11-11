#pragma once

#include <emb/container/circular_buffer.hpp>
#include <emb/filters/exponential.hpp>
#include <emb/filters/median.hpp>
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

  friend constexpr bool operator==(angle_sensor_config const&,
                                   angle_sensor_config const&) = default;
};

constexpr std::expected<void, error> validate(angle_sensor_config const& conf)
{
  if (!(conf.speed_timeconstant.value > 0)) {
    return std::unexpected(error::invalid_config);
  }
  return {};
}

// The class template `angle_sensor` estimates the electrical angle and speed
// of a rotor from three hall sensors that the capture timer `Timer` reads. It
// does not check the config and the calibration it is given: the behavior is
// undefined if one passed to the constructor, to `configure` or to
// `apply_calibration` fails `validate`.
template<some_capture_timer Timer>
class angle_sensor {
private:
  angle_sensor_config conf_;

  emb::units::sec_f32 timestep_;
  Timer& capture_timer_;
  std::optional<emb::units::sec_f32> new_transition_period_ = std::nullopt;

  sector_geometry geometry_{};

  // sectors recognized in the last two pollings
  emb::circular_buffer<sector, 2> sector_history_{};

  emb::filters::median<emb::units::eradps_f32, 3> speedmedian_{
      emb::units::eradps_f32{0}};
  emb::units::eradps_f32 speed_{0};

  sector_span sector_span_{};
  emb::units::erad_f32 offset_{};
  emb::units::erad_f32 angle_{};
public:
  angle_sensor(some_timebase auto const& timebase,
               Timer& capture_timer,
               angle_sensor_config const& sensor_conf,
               calibration_result const& calibration)
      : conf_(sensor_conf),
        timestep_(timebase.period()),
        capture_timer_(capture_timer)
  {
    geometry_ = make_geometry(calibration);

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

  [[nodiscard]] std::optional<error> step()
  {
    if (new_transition_period_) {
      return on_new_transition();
    }

    offset_ += speed_ * timestep_;
    if (offset_.value >= 0) {
      offset_ = std::min(offset_, sector_span_.width);
    }
    else {
      offset_ = std::max(offset_, -sector_span_.width);
    }
    angle_ = sector_span_.entry + offset_;
    return {};
  }

  [[nodiscard]] std::optional<error> on_timeout()
  {
    offset_ = emb::units::erad_f32{0};
    angle_ = sector_span_.entry + sector_halfwidth;
    speedmedian_.reset(emb::units::eradps_f32{0});
    speed_ = emb::units::eradps_f32{0};

    if (!capture_sector().has_value()) {
      return error::invalid_input;
    }
    return std::nullopt;
  }

  emb::units::erad_f32 angle() const
  {
    return emb::norm2pi_fast(angle_);
  }

  emb::units::eradps_f32 speed() const
  {
    return speed_;
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

  void configure(angle_sensor_config const& conf)
  {
    conf_ = conf;
  }

  void apply_calibration(calibration_result const& data)
  {
    geometry_ = make_geometry(data);
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
    speedmedian_.update(raw_speed);
    // each transition has its own step, so a filter lasts for one value
    emb::filters::exponential smoother{transition_period,
                                       conf_.speed_timeconstant,
                                       speed_};
    smoother.update(speedmedian_.output());
    speed_ = smoother.output();

    sector_span_ = sectors[sector_to];
    offset_ = speed_ * time_since_transition;
    angle_ = sector_span_.entry + offset_;

    return std::nullopt;
  }
};

} // namespace emb::hall
