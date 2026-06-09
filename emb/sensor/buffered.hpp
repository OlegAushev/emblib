#pragma once

#include <emb/sensor/concepts.hpp>
#include <emb/sensor/unbuffered.hpp>

#include <utility>

namespace emb::sensor {

// Buffered sensor: an SPSC queue decouples the producing context (ISR, via
// submit) from the consuming context (main loop, via process). process drains
// the queue through an unbuffered core, so conversion/filtering happens off the
// ISR. The raw sample type is taken from the queue.
template<typename Queue, typename Converter, typename Filter>
  requires some_spsc_queue<Queue>
        && some_filter<Filter>
        && some_converter<
               Converter,
               typename Queue::value_type,
               typename Filter::value_type>
class buffered {
public:
  using rawdata_type = typename Queue::value_type;
  using value_type = typename Filter::value_type;
private:
  Queue queue_;
  unbuffered<Converter, Filter, rawdata_type> core_;
public:
  buffered(Converter converter, Filter filter)
      : core_(std::move(converter), std::move(filter)) {}

  value_type value() const {
    return core_.value();
  }

  // Producer-side (ISR). On overflow the newest sample is dropped.
  void submit(rawdata_type data) {
    [[maybe_unused]] auto r = queue_.try_push(data);
  }

  // Consumer-side (main loop). Drains the queue through the core pipeline.
  void process() {
    while (auto const raw = queue_.try_pop()) {
      core_.submit(*raw);
    }
  }
};

} // namespace emb::sensor
