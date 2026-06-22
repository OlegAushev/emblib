#pragma once

#include <emb/sensor/concepts.hpp>

#include <cstdint>
#include <utility>

namespace emb::sensor {

// Single-phase transport-free sensor core: conversion and filtering run
// immediately in the caller's context (e.g. the same ISR that produces the
// sample). No queue and no deferred process() step. See polyphase for the
// N-phase counterpart and buffered for the queued decorator over either.
template<typename Converter, typename Filter, typename RawData = std::uint32_t>
  requires some_filter<Filter>
        && some_converter<Converter, RawData, typename Filter::value_type>
class singlephase {
public:
  using rawdata_type = RawData; // one channel's raw code
  using sample_type = RawData;  // one acquisition: for a single phase, the code
  using value_type = typename Filter::value_type;
private:
  Converter converter_;
  Filter filter_;
public:
  singlephase(Converter converter, Filter filter)
      : converter_(std::move(converter)), filter_(std::move(filter)) {}

  value_type value() const {
    return filter_.output();
  }

  // Converts and filters in place; safe to call from the producing context.
  void submit(sample_type data) {
    filter_.push(converter_(data));
  }
};

} // namespace emb::sensor
