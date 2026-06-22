#pragma once

#include <concepts>
#include <optional>

namespace emb::sensor {

template<typename Q>
concept some_spsc_queue = requires(Q q, typename Q::value_type v) {
  typename Q::value_type;
  { q.try_push(v) } -> std::convertible_to<bool>;
  { q.try_pop() } -> std::same_as<std::optional<typename Q::value_type>>;
};

template<typename P>
concept some_prefilter = requires(P p, typename P::sample_type const& s) {
  typename P::sample_type;
  typename P::value_type;
  { p(s) } -> std::convertible_to<typename P::value_type>;
};

template<typename C, typename Input, typename Output>
concept some_converter = requires(C c, Input in) {
  { c(in) } -> std::convertible_to<Output>;
};

template<typename F>
concept some_filter = requires(
    F f,
    F const cf,
    typename F::value_type const v
) {
  typename F::value_type;
  { cf.output() } -> std::convertible_to<typename F::value_type>;
  f.push(v);
};

// A sensor core: it converts and filters submitted samples and exposes the
// filtered result. sample_type is what a producer submits and what a buffered
// queue stores -- one scalar code for singlephase, one aligned frame of codes
// for polyphase. raw_type is the per-channel scalar code (equal to sample_type
// for singlephase). buffered composes any such core.
template<typename C>
concept some_sensor_core = requires(C c, typename C::sample_type const& s) {
  typename C::sample_type;
  typename C::raw_type;
  typename C::value_type;
  c.submit(s);
};

} // namespace emb::sensor
