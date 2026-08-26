#pragma once

#include <emb/sensor/concepts.hpp>

#include <utility>

namespace emb::sensor {

// Single-phase transport-free sensor core: conversion and filtering run
// immediately in the caller's context (e.g. the same ISR that produces the
// sample). No queue and no deferred process() step. See polyphase for the
// N-phase counterpart and buffered for the queued decorator over either.
template<typename Raw, typename Converter, typename Filter>
  requires some_filter<Filter>
        && some_converter<Converter, Raw, typename Filter::value_type>
class singlephase {
public:
  using sample_type = Raw;
  using raw_type = Raw;
  using value_type = typename Filter::value_type;
  using sensor_category = immediate_tag;
private:
  Converter converter_;
  Filter filter_;
public:
  // Available if both stages are default-constructible; otherwise
  // implicitly deleted, so stages carrying calibration or runtime state
  // still force explicit construction.
  singlephase() = default;

  singlephase(Converter converter, Filter filter)
      : converter_(std::move(converter)), filter_(std::move(filter)) {}

  value_type value() const {
    return filter_.output();
  }

  void submit(sample_type sample) {
    filter_.push(converter_(sample));
  }
};

} // namespace emb::sensor
