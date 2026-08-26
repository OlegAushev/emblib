#pragma once

#include <emb/sensor/concepts.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <utility>

namespace emb::sensor {

// N-source sensor over one time-shared acquisition path, e.g. an analog
// multiplexer in front of a single ADC channel: an aggregate of N
// independent singlephase sensors, one per source, fed one source at a time.
// Unlike polyphase there is no frame -- the sources are sampled in turn, so
// their readings belong to different instants, and the producer tags every
// sample with the source it was taken from. Which source is selected when,
// and how long it settles, is the producer's business.
//
// Deliberately not a some_sensor: submit needs the source alongside the
// sample, so multiplexed cannot sit inside a buffered decorator. Decorate the
// per-source sensor instead: multiplexed<buffered<...>, N> queues per source
// and drains them all through process(). N == 1 is a path wired straight to
// its only source.
template<typename Sensor, std::size_t N>
  requires some_singlephase_sensor<Sensor> && (N > 0)
class multiplexed {
public:
  using sensor_type = Sensor;
  using sample_type = typename Sensor::sample_type;
  using raw_type = typename Sensor::raw_type;
  using value_type = typename Sensor::value_type;
  using values_type = std::array<value_type, N>;
  // Forwarded for introspection, like value_type; multiplexed itself is not a
  // some_sensor (see above).
  using sensor_category = typename Sensor::sensor_category;

  static constexpr std::size_t source_count = N;
private:
  std::array<Sensor, N> sensors_;

  template<typename... Args, std::size_t... I>
  constexpr multiplexed(std::index_sequence<I...>, Args const&... args)
      : sensors_{((void)I, Sensor(args...))...} {}
public:
  constexpr multiplexed() = default;

  // Broadcast construction: every source's sensor is built from the same
  // arguments. Each source still keeps its own independent state.
  template<typename... Args>
    requires(sizeof...(Args) > 0)
         && std::constructible_from<Sensor, Args const&...>
  constexpr explicit multiplexed(Args const&... args)
      : multiplexed(std::make_index_sequence<N>{}, args...) {}

  constexpr Sensor const& sensor(std::size_t source) const {
    return sensors_[source];
  }

  // Producer-side: the sample taken while `source` was selected.
  constexpr void submit(std::size_t source, sample_type sample) {
    sensors_[source].submit(std::move(sample));
  }

  // Consumer-side, present when the per-source sensor defers work to a
  // process() step (buffered): drains every source.
  constexpr void process()
    requires requires(Sensor& s) { s.process(); } {
    for (auto& sensor : sensors_) {
      sensor.process();
    }
  }

  constexpr value_type value(std::size_t source) const {
    return sensors_[source].value();
  }

  constexpr values_type values() const {
    return values_impl(std::make_index_sequence<N>{});
  }
private:
  template<std::size_t... I>
  constexpr values_type values_impl(std::index_sequence<I...>) const {
    return {sensors_[I].value()...};
  }
};

} // namespace emb::sensor
