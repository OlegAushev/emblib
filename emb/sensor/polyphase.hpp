#pragma once

#include <emb/sensor/concepts.hpp>
#include <emb/sensor/singlephase.hpp>

#include <array>
#include <cstddef>
#include <utility>

namespace emb::sensor {

// N-phase sensor core: an aggregate of N independent singlephase cores driven
// by one acquisition frame. Adds only frame-synchronous submission, so
// the N filtered outputs read back from value()/values() belong to the same
// sampling instant.
template<typename Prefilter, typename Converter, typename Filter, std::size_t N>
  requires some_prefilter<Prefilter>
        && some_filter<Filter>
        && some_converter<
               Converter,
               typename Prefilter::value_type,
               typename Filter::value_type>
        && (N > 0)
class polyphase {
public:
  using core_type = singlephase<Prefilter, Converter, Filter>;
  using sample_type = std::array<typename core_type::sample_type, N>;
  using raw_type = typename Prefilter::value_type;
  using value_type = typename Filter::value_type;
  using values_type = std::array<value_type, N>;

  static constexpr std::size_t phase_count = N;
private:
  std::array<core_type, N> cores_;

  template<std::size_t... I>
  polyphase(
      std::array<Prefilter, N> prefilters,
      std::array<Converter, N> converters,
      std::array<Filter, N> filters,
      std::index_sequence<I...>
  )
      : cores_{core_type(
            std::move(prefilters[I]),
            std::move(converters[I]),
            std::move(filters[I])
        )...} {}
public:
  // Per-phase construction: each phase gets its own prefilter, converter and
  // filter, e.g. independent per-phase gain/offset calibration.
  polyphase(
      std::array<Prefilter, N> prefilters,
      std::array<Converter, N> converters,
      std::array<Filter, N> filters
  )
      : polyphase(
            std::move(prefilters),
            std::move(converters),
            std::move(filters),
            std::make_index_sequence<N>{}
        ) {}

  // Broadcast construction: one prefilter/converter/filter prototype copied
  // into every phase. Each phase still keeps its own independent state.
  polyphase(Prefilter prefilter, Converter converter, Filter filter)
      : polyphase(
            broadcast(prefilter),
            broadcast(converter),
            broadcast(filter),
            std::make_index_sequence<N>{}
        ) {}

  value_type value(std::size_t phase) const {
    return cores_[phase].value();
  }

  values_type values() const {
    return values_impl(std::make_index_sequence<N>{});
  }

  core_type const& core(std::size_t phase) const {
    return cores_[phase];
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
