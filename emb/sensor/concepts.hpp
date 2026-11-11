#pragma once

#include <emb/filters/concepts.hpp>

#include <concepts>
#include <optional>

namespace emb::sensor {

template<typename Q>
concept some_spsc_queue = requires(Q q, typename Q::value_type v) {
  typename Q::value_type;
  { q.try_push(v) } -> std::convertible_to<bool>;
  { q.try_pop() } -> std::same_as<std::optional<typename Q::value_type>>;
};

// Sensor categories, on one axis: when conversion completes. An immediate
// sensor converts and filters inside update(input), in the caller's context;
// a deferred sensor parks the input in push(input) and converts later, in a
// consumer-side update(). The category is declared by the type's author.
struct immediate_tag {};
struct deferred_tag {};

// Any sensor surface: typed input/output and a declared category, which sets
// the verbs that take the input. input_type is what a producer hands over and
// what a buffered queue stores -- one sample for singlechannel, one aligned
// frame of samples for multichannel.
template<typename S>
concept some_sensor = requires {
  typename S::input_type;
  typename S::output_type;
  typename S::sensor_category;
};

template<typename S>
concept some_immediate_sensor =
    some_sensor<S>
    && std::same_as<typename S::sensor_category, immediate_tag>
    && requires(S s, typename S::input_type const& input) { s.update(input); };

template<typename S>
concept some_deferred_sensor =
    some_sensor<S>
    && std::same_as<typename S::sensor_category, deferred_tag>
    && requires(S s, typename S::input_type const& input) {
         s.push(input);
         s.update();
       };

template<typename S>
concept some_singlechannel_sensor =
    some_sensor<S> && requires(S const& s) { s.output(); };

template<typename S>
concept some_multichannel_sensor =
    some_sensor<S> && requires(S const& s) { s.outputs(); };

} // namespace emb::sensor
