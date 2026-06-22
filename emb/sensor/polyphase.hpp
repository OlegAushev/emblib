#pragma once

#include <emb/sensor/concepts.hpp>
#include <emb/sensor/singlephase.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace emb::sensor {

// N-phase sensor core: an aggregate of N independent singlephase cores driven
// by one acquisition frame. Each phase converts and filters on its own, so the
// per-phase value_type stays scalar and the some_filter contract is unchanged;
// polyphase only adds frame-synchronous submission, so the N filtered outputs
// read back from value()/values() belong to the same sampling instant.
template<
    typename Converter,
    typename Filter,
    std::size_t N,
    typename RawData = std::uint32_t>
  requires some_filter<Filter>
        && some_converter<Converter, RawData, typename Filter::value_type>
        && (N > 0)
class polyphase {
public:
  using core_type = singlephase<Converter, Filter, RawData>;
  using rawdata_type = RawData;                 // one phase's raw code
  using sample_type = std::array<RawData, N>;   // one aligned acquisition frame
  using frame_type = sample_type;               // readable alias at call sites
  using value_type = typename Filter::value_type;
  using values_type = std::array<value_type, N>;

  static constexpr std::size_t phase_count = N;
private:
  std::array<core_type, N> cores_;

  template<std::size_t... I>
  polyphase(
      std::array<Converter, N> converters,
      std::array<Filter, N> filters,
      std::index_sequence<I...>
  )
      : cores_{
            core_type(std::move(converters[I]), std::move(filters[I]))...} {}
public:
  // Per-phase construction: each phase gets its own converter and filter, e.g.
  // independent per-phase gain/offset calibration.
  polyphase(std::array<Converter, N> converters, std::array<Filter, N> filters)
      : polyphase(
            std::move(converters),
            std::move(filters),
            std::make_index_sequence<N>{}) {}

  // Broadcast construction: one converter/filter prototype copied into every
  // phase. Each phase still keeps its own independent filter state.
  polyphase(Converter converter, Filter filter)
      : polyphase(
            broadcast(converter),
            broadcast(filter),
            std::make_index_sequence<N>{}) {}

  // Filtered output of one phase; all phases reflect the same submitted frame.
  value_type value(std::size_t phase) const {
    return cores_[phase].value();
  }

  values_type values() const {
    return values_impl(std::make_index_sequence<N>{});
  }

  // Access to an individual phase core (e.g. to read one phase in isolation).
  core_type const& core(std::size_t phase) const {
    return cores_[phase];
  }

  // Converts and filters one aligned frame in place; safe to call from the
  // producing context (e.g. the ADC-complete ISR that fills the frame).
  void submit(frame_type const& frame) {
    submit_impl(frame, std::make_index_sequence<N>{});
  }
private:
  template<typename T>
  static std::array<T, N> broadcast(T const& proto) {
    return broadcast_impl(proto, std::make_index_sequence<N>{});
  }

  template<typename T, std::size_t... I>
  static std::array<T, N> broadcast_impl(
      T const& proto, std::index_sequence<I...>
  ) {
    return {((void)I, proto)...};
  }

  template<std::size_t... I>
  void submit_impl(frame_type const& frame, std::index_sequence<I...>) {
    (cores_[I].submit(frame[I]), ...);
  }

  template<std::size_t... I>
  values_type values_impl(std::index_sequence<I...>) const {
    return {cores_[I].value()...};
  }
};

} // namespace emb::sensor
