#pragma once

#include <emb/sensor/concepts.hpp>

#include <cstdint>
#include <utility>

namespace emb::sensor {

// Transport-free sensor core: conversion and filtering run immediately in the
// caller's context (e.g. the same ISR that produces the sample). No queue and
// no deferred process() step. RawData is a template parameter since, without a
// queue, the raw sample type cannot be deduced.
template<typename Converter, typename Filter, typename RawData = std::uint32_t>
  requires some_filter<Filter>
        && some_converter<Converter, RawData, typename Filter::value_type>
class unbuffered {
public:
  using rawdata_type = RawData;
  using value_type = typename Filter::value_type;
private:
  Converter converter_;
  Filter filter_;
public:
  unbuffered(Converter converter, Filter filter)
      : converter_(std::move(converter)), filter_(std::move(filter)) {}

  value_type value() const {
    return filter_.output();
  }

  // Converts and filters in place; safe to call from the producing context.
  void submit(rawdata_type data) {
    filter_.push(converter_(data));
  }
};

} // namespace emb::sensor
