#pragma once

#include <emb/sensor/concepts.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <utility>

namespace emb::sensor {

// N-channel sensor core: an aggregate of N independent singlechannel cores
// driven by one acquisition frame. Adds only frame-synchronous submission, so
// the N filtered outputs read back from value()/values() belong to the same
// acquisition frame.
template<typename Core, std::size_t N>
  requires some_singlechannel_sensor<Core>
        && some_immediate_sensor<Core>
        && (N > 0)
class multichannel {
public:
  using core_type = Core;
  using raw_type = typename Core::raw_type;
  using sample_type = std::array<typename Core::sample_type, N>;
  using value_type = typename Core::value_type;
  using values_type = std::array<value_type, N>;
  using sensor_category = immediate_tag;

  static constexpr std::size_t channel_count = N;
private:
  std::array<Core, N> cores_;

  template<typename... Args, std::size_t... I>
  constexpr multichannel(std::index_sequence<I...>, Args const&... args)
      : cores_{((void)I, Core(args...))...} {}
public:
  constexpr multichannel() = default;

  // Per-channel construction: each channel gets its own core, e.g. with
  // independent per-channel gain/offset calibration.
  constexpr explicit multichannel(std::array<Core, N> cores)
      : cores_(std::move(cores)) {}

  // Broadcast construction: every channel's core is built from the same
  // arguments. Each channel still keeps its own independent state.
  template<typename... Args>
    requires(sizeof...(Args) > 0)
         && std::constructible_from<Core, Args const&...>
  constexpr explicit multichannel(Args const&... args)
      : multichannel(std::make_index_sequence<N>{}, args...) {}

  constexpr value_type value(std::size_t channel) const {
    return cores_[channel].value();
  }

  constexpr values_type values() const {
    return values_impl(std::make_index_sequence<N>{});
  }

  constexpr core_type const& core(std::size_t channel) const {
    return cores_[channel];
  }

  constexpr core_type& core(std::size_t channel) {
    return cores_[channel];
  }

  constexpr void submit(sample_type const& sample) {
    submit_impl(sample, std::make_index_sequence<N>{});
  }
private:
  template<std::size_t... I>
  constexpr void
  submit_impl(sample_type const& sample, std::index_sequence<I...>) {
    (cores_[I].submit(sample[I]), ...);
  }

  template<std::size_t... I>
  constexpr values_type values_impl(std::index_sequence<I...>) const {
    return {cores_[I].value()...};
  }
};

} // namespace emb::sensor
