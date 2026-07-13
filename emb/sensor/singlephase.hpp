#pragma once

#include <emb/sensor/concepts.hpp>

#include <utility>

namespace emb::sensor {

// Single-phase transport-free sensor core: conversion and filtering run
// immediately in the caller's context (e.g. the same ISR that produces the
// sample). No queue and no deferred process() step. See polyphase for the
// N-phase counterpart and buffered for the queued decorator over either.
template<typename Prefilter, typename Converter, typename Filter>
  requires some_prefilter<Prefilter>
        && some_filter<Filter>
        && some_converter<
               Converter,
               typename Prefilter::value_type,
               typename Filter::value_type>
class singlephase {
public:
  using sample_type = typename Prefilter::sample_type;
  using raw_type = typename Prefilter::value_type;
  using value_type = typename Filter::value_type;
private:
  Prefilter prefilter_;
  Converter converter_;
  Filter filter_;
public:
  singlephase(Prefilter prefilter, Converter converter, Filter filter)
      : prefilter_(std::move(prefilter)),
        converter_(std::move(converter)),
        filter_(std::move(filter)) {}

  value_type value() const {
    return filter_.output();
  }

  void submit(sample_type sample) {
    filter_.push(converter_(prefilter_(sample)));
  }
};

} // namespace emb::sensor
