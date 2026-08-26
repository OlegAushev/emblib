#pragma once

#include <emb/sensor/concepts.hpp>
#include <emb/sensor/singlechannel.hpp>

#include <array>
#include <cstddef>
#include <utility>

namespace emb::sensor {

// N-channel sensor core: an aggregate of N independent singlechannel cores
// driven by one acquisition frame. Adds only frame-synchronous submission, so
// the N filtered outputs read back from value()/values() belong to the same
// acquisition frame.
template<typename Raw, typename Converter, typename Filter, std::size_t N>
  requires some_filter<Filter>
        && some_converter<Converter, Raw, typename Filter::value_type>
        && (N > 0)
class multichannel {
public:
  using core_type = singlechannel<Raw, Converter, Filter>;
  using sample_type = std::array<typename core_type::sample_type, N>;
  using raw_type = Raw;
  using value_type = typename Filter::value_type;
  using values_type = std::array<value_type, N>;
  using sensor_category = immediate_tag;

  static constexpr std::size_t channel_count = N;
private:
  std::array<core_type, N> cores_;

  template<std::size_t... I>
  multichannel(
      std::array<Converter, N> converters,
      std::array<Filter, N> filters,
      std::index_sequence<I...>
  )
      : cores_{core_type(std::move(converters[I]), std::move(filters[I]))...} {}
public:
  multichannel() = default;

  // Per-channel construction: each channel gets its own converter and filter,
  // e.g. independent per-channel gain/offset calibration.
  multichannel(
      std::array<Converter, N> converters,
      std::array<Filter, N> filters
  )
      : multichannel(
            std::move(converters),
            std::move(filters),
            std::make_index_sequence<N>{}
        ) {}

  // Broadcast construction: one converter/filter prototype copied into every
  // channel. Each channel still keeps its own independent state.
  multichannel(Converter converter, Filter filter)
      : multichannel(
            broadcast(converter),
            broadcast(filter),
            std::make_index_sequence<N>{}
        ) {}

  value_type value(std::size_t channel) const {
    return cores_[channel].value();
  }

  values_type values() const {
    return values_impl(std::make_index_sequence<N>{});
  }

  core_type const& core(std::size_t channel) const {
    return cores_[channel];
  }

  void submit(sample_type const& sample) {
    submit_impl(sample, std::make_index_sequence<N>{});
  }
private:
  template<typename T>
  static std::array<T, N> broadcast(T const& proto) {
    return broadcast_impl(proto, std::make_index_sequence<N>{});
  }

  template<typename T, std::size_t... I>
  static std::array<T, N>
  broadcast_impl(T const& proto, std::index_sequence<I...>) {
    return {((void)I, proto)...};
  }

  template<std::size_t... I>
  void submit_impl(sample_type const& sample, std::index_sequence<I...>) {
    (cores_[I].submit(sample[I]), ...);
  }

  template<std::size_t... I>
  values_type values_impl(std::index_sequence<I...>) const {
    return {cores_[I].value()...};
  }
};

} // namespace emb::sensor
