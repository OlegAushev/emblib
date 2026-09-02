#pragma once

#include <emb/sensor/concepts.hpp>

#include <utility>

namespace emb::sensor {

// Single-channel transport-free sensor core: conversion and filtering run
// immediately in the caller's context (e.g. the same ISR that produces the
// sample). No queue and no deferred process() step. See multichannel for the
// N-channel counterpart and buffered for the queued decorator over either.
//
// Stages are reachable through converter()/filter(): const for
// introspection, non-const for post-construction state -- calibration
// carried by converter stages, filter preload or retuning. Mutation belongs
// to the context that runs the conversion (submit() here, the consumer-side
// process() once wrapped in buffered), or to a sensor that is quiescent.
// multichannel, buffered and multiplexed expose their parts under the same
// rule.
template<typename Raw, typename Converter, typename Filter>
  requires some_filter<Filter>
        && some_converter<Converter, Raw, typename Filter::value_type>
class singlechannel {
public:
  using raw_type = Raw;
  using sample_type = Raw;
  using value_type = typename Filter::value_type;
  using sensor_category = immediate_tag;
private:
  Converter converter_;
  Filter filter_;
public:
  constexpr singlechannel() = default;

  constexpr singlechannel(Converter converter, Filter filter)
      : converter_(std::move(converter)), filter_(std::move(filter))
  {
  }

  constexpr Converter const& converter() const
  {
    return converter_;
  }

  constexpr Converter& converter()
  {
    return converter_;
  }

  constexpr Filter const& filter() const
  {
    return filter_;
  }

  constexpr Filter& filter()
  {
    return filter_;
  }

  constexpr value_type value() const
  {
    return filter_.output();
  }

  constexpr void submit(sample_type sample)
  {
    filter_.push(converter_(sample));
  }
};

} // namespace emb::sensor
