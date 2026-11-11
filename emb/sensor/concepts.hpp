#pragma once

#include <emb/filter/concepts.hpp>

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

// Sensor categories, on one axis: when conversion completes. An immediate
// sensor converts and filters inside update(sample), in the caller's context;
// a deferred sensor parks the sample in push(sample) and converts later, in a
// consumer-side update(). The category is declared by the type's author.
struct immediate_tag {};
struct deferred_tag {};

// Any sensor surface: typed raw/sample/value and a declared category, which
// sets the verbs that take samples. sample_type is what a producer hands over
// and what a buffered queue stores -- one scalar value for singlechannel, one
// aligned frame of values for multichannel. raw_type is the per-channel scalar
// value (equal to sample_type for singlechannel).
template<typename S>
concept some_sensor = requires {
  typename S::raw_type;
  typename S::sample_type;
  typename S::value_type;
  typename S::sensor_category;
};

template<typename S>
concept some_immediate_sensor =
    some_sensor<S>
    && std::same_as<typename S::sensor_category, immediate_tag>
    && requires(S s, typename S::sample_type const& sample) {
         s.update(sample);
       };

template<typename S>
concept some_deferred_sensor =
    some_sensor<S>
    && std::same_as<typename S::sensor_category, deferred_tag>
    && requires(S s, typename S::sample_type const& sample) {
         s.push(sample);
         s.update();
       };

template<typename S>
concept some_singlechannel_sensor =
    some_sensor<S> && requires(S const& s) { s.value(); };

template<typename S>
concept some_multichannel_sensor =
    some_sensor<S> && requires(S const& s) { s.values(); };

} // namespace emb::sensor
