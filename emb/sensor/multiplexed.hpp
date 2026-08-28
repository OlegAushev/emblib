#pragma once

#include <emb/meta.hpp>
#include <emb/sensor/concepts.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace emb::sensor {

// N-source sensor over one time-shared acquisition path, e.g. an analog
// multiplexer in front of a single ADC channel: an aggregate of N
// independent singlechannel sensors, one per source, fed one source at a time.
// Unlike multichannel there is no frame -- the sources are sampled in turn, so
// their readings belong to different instants, and the producer tags every
// sample with the source it was taken from. Which source is selected when,
// and how long it settles, is the producer's business.
//
// The sources may be different sensor types but they share the path, so they
// share sample_type and raw_type, and they share the producer and consumer,
// so they share sensor_category.
// Two independent kinds of uniformity are detected and exploited:
//   - uniform_sensors: every source is the same type. Sources live in an
//     array and are addressed by index directly; sensor(source) is available.
//   - uniform_values: every source yields the same value_type, whether or not
//     the sensors are the same type. values() is an array and value(source)
//     is available; otherwise values() is a tuple and sources are read
//     through value<I>().
//
// Deliberately not a some_sensor: submit needs the source alongside the
// sample, so multiplexed cannot sit inside a buffered decorator. Decorate the
// per-source sensor instead: multiplexed<buffered<...>...> queues per source
// and drains them all through process(). A single source is a path wired
// straight to it.
template<typename... Sensors>
  requires(sizeof...(Sensors) > 0)
       && (some_singlechannel_sensor<Sensors> && ...)
       && all_same<typename Sensors::raw_type...>
       && all_same<typename Sensors::sample_type...>
       && all_same<typename Sensors::sensor_category...>
class multiplexed {
  using first_type = nth_type_t<0, Sensors...>;
public:
  static constexpr std::size_t source_count = sizeof...(Sensors);
  static constexpr bool uniform_sensors = all_same<Sensors...>;
  static constexpr bool uniform_values =
      all_same<typename Sensors::value_type...>;

  template<std::size_t I>
  using sensor_type = nth_type_t<I, Sensors...>;

  using raw_type = typename first_type::raw_type;
  using sample_type = typename first_type::sample_type;
  // Forwarded for introspection; multiplexed itself is not a some_sensor
  // (see above).
  using sensor_category = typename first_type::sensor_category;
  using sensors_type = std::conditional_t<
      uniform_sensors,
      std::array<first_type, source_count>,
      std::tuple<Sensors...>>;
  using values_type = std::conditional_t<
      uniform_values,
      std::array<typename first_type::value_type, source_count>,
      std::tuple<typename Sensors::value_type...>>;
private:
  sensors_type sensors_;

  template<typename... Args, std::size_t... I>
  constexpr multiplexed(std::index_sequence<I...>, Args const&... args)
      : sensors_{((void)I, first_type(args...))...} {}
public:
  constexpr multiplexed() = default;

  // Per-source construction: each source gets its own sensor, e.g. with a
  // different transducer model or calibration behind the same multiplexer.
  // Sensors are moved in, so a non-movable sensor (buffered) is limited to
  // default construction.
  constexpr explicit multiplexed(sensors_type sensors)
    requires(std::move_constructible<Sensors> && ...)
      : sensors_(std::move(sensors)) {}

  // Broadcast construction: every source's sensor is built from the same
  // arguments. Each source still keeps its own independent state. Uniform
  // sources only: sensors of different types differ in what they are built
  // from. The array is initialized from prvalues, so sensors need not be
  // movable.
  template<typename... Args>
    requires uniform_sensors && (sizeof...(Args) > 0)
          && std::constructible_from<first_type, Args const&...>
  constexpr explicit multiplexed(Args const&... args)
      : multiplexed(std::make_index_sequence<source_count>{}, args...) {}

  template<std::size_t I>
  constexpr sensor_type<I> const& sensor() const {
    return std::get<I>(sensors_);
  }

  template<std::size_t I>
  constexpr sensor_type<I>& sensor() {
    return std::get<I>(sensors_);
  }

  constexpr first_type const& sensor(std::size_t source) const
    requires uniform_sensors {
    return sensors_[source];
  }

  constexpr first_type& sensor(std::size_t source)
    requires uniform_sensors {
    return sensors_[source];
  }

  // Producer-side: the sample taken while `source` was selected. Uniform
  // sources are indexed directly, otherwise selected through visit_at's
  // compare chain; `source` must be below source_count.
  constexpr void submit(std::size_t source, sample_type sample) {
    visit_at(sensors_, source, [&](auto& sensor) {
      sensor.submit(std::move(sample));
    });
  }

  // Consumer-side, present when the per-source sensors defer work to a
  // process() step (buffered): drains every source.
  constexpr void process()
    requires(requires(Sensors& s) { s.process(); } && ...) {
    if constexpr (uniform_sensors) {
      for (auto& sensor : sensors_) {
        sensor.process();
      }
    } else {
      unroll<source_count>([&]<std::size_t I>() {
        std::get<I>(sensors_).process();
      });
    }
  }

  template<std::size_t I>
  constexpr typename sensor_type<I>::value_type value() const {
    return std::get<I>(sensors_).value();
  }

  constexpr typename first_type::value_type value(std::size_t source) const
    requires uniform_values {
    return visit_at(sensors_, source, [](auto const& sensor) {
      return sensor.value();
    });
  }

  constexpr values_type values() const {
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
      return values_type{std::get<I>(sensors_).value()...};
    }(std::make_index_sequence<source_count>{});
  }
};

// multiplexed<Sensor, Sensor, ...>: N sources of one sensor type, for a
// source count known as a constant rather than a spelled-out list, e.g. a
// board's multiplexer input count.
template<typename Sensor, std::size_t N>
  requires(N > 0)
using multiplexed_n = replicate_t<multiplexed, Sensor, N>;

} // namespace emb::sensor
