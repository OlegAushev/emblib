#pragma once

#include <emb/sensors/concepts.hpp>

#include <concepts>
#include <type_traits>
#include <utility>

namespace emb::sensors {

// Single-channel transport-free sensor core: conversion and filtering run
// immediately in the caller's context (e.g. the same ISR that produces the
// sample). No queue and no consumer-side update(). See multichannel for the
// N-channel counterpart and buffered for the queued decorator over either.
//
// Stages are reachable through converter()/filter(): const for
// introspection, non-const for post-construction state -- calibration
// carried by converter stages, filter preload or retuning. Mutation belongs
// to the context that runs the conversion (update(sample) here, the
// consumer-side update() once wrapped in buffered), or to a sensor that is
// quiescent. multichannel, buffered and multiplexed expose their parts under
// the same rule.
template<typename Sample, typename Converter, typename Filter>
  requires filters::filter<Filter>
        && std::invocable<Converter&, Sample&>
        && std::convertible_to<std::invoke_result_t<Converter&, Sample&>,
                               typename Filter::value_type>
class singlechannel {
public:
  using input_type = Sample;
  using output_type = typename Filter::value_type;
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

  constexpr output_type output() const
  {
    return filter_.output();
  }

  constexpr void update(input_type sample)
  {
    filter_.update(converter_(sample));
  }
};

} // namespace emb::sensors
