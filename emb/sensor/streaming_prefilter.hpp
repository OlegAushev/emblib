#pragma once

#include <emb/sensor/concepts.hpp>

#include <emb/filter/passthrough_filter.hpp>

#include <cstdint>
#include <utility>

namespace emb::sensor {

// Adapts any some_filter into a some_prefilter: each submitted scalar is pushed
// through the wrapped filter and its output taken immediately.
// This reuses the existing filters as raw-domain prefilters without a parallel
// hierarchy:
//   streaming_prefilter<passthrough_filter<T>>       -- no-op identity
//   streaming_prefilter<moving_average_filter<T, K>> -- sliding raw average
// Block (pack-in) reduction is a separate, native some_prefilter, not this.
template<some_filter Filter>
class streaming_prefilter {
private:
  Filter filter_;
public:
  using sample_type = typename Filter::value_type;
  using value_type = typename Filter::value_type;

  constexpr streaming_prefilter() = default;
  constexpr explicit streaming_prefilter(Filter filter)
      : filter_(std::move(filter)) {}

  constexpr value_type operator()(sample_type sample) {
    filter_.push(sample);
    return filter_.output();
  }
};

// The no-op prefilter: passes raw codes straight to the converter. Use it
// as the prefilter argument when a sensor needs no raw-domain preprocessing.
template<typename T>
using passthrough_prefilter = streaming_prefilter<passthrough_filter<T>>;

} // namespace emb::sensor
