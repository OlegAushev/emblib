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

template<typename C, typename Input, typename Output>
concept some_converter = requires(C c, Input in) {
  { c(in) } -> std::convertible_to<Output>;
};

template<typename F>
concept some_filter = requires(F f,
                               F const cf,
                               typename F::value_type const v) {
  typename F::value_type;
  { cf.output() } -> std::convertible_to<typename F::value_type>;
  f.push(v);
};

// Sensor categories, on one axis: when conversion completes. An immediate
// sensor converts and filters inside submit(), in the caller's context; a
// deferred sensor parks the sample in submit() and converts later, in a
// consumer-side process(). The category is declared by the type's author.
struct immediate_tag {};
struct deferred_tag {};

// Any sensor surface: typed raw/sample/value, a declared category, and a
// one-argument submit. sample_type is what a producer submits and what a
// buffered queue stores -- one scalar value for singlechannel, one aligned
// frame of values for multichannel. raw_type is the per-channel scalar value
// (equal to sample_type for singlechannel).
template<typename S>
concept some_sensor = requires(S s, typename S::sample_type const& sample) {
  typename S::raw_type;
  typename S::sample_type;
  typename S::value_type;
  typename S::sensor_category;
  s.submit(sample);
};

template<typename S>
concept some_immediate_sensor =
    some_sensor<S> && std::same_as<typename S::sensor_category, immediate_tag>;

template<typename S>
concept some_deferred_sensor =
    some_sensor<S> && std::same_as<typename S::sensor_category, deferred_tag>;

template<typename S>
concept some_singlechannel_sensor = some_sensor<S>
                                 && requires(S const& s) { s.value(); };

template<typename S>
concept some_multichannel_sensor = some_sensor<S>
                                && requires(S const& s) { s.values(); };

} // namespace emb::sensor
