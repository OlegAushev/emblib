#pragma once

#include <emb/sensor/concepts.hpp>
#include <emb/sensor/singlephase.hpp>

#include <concepts>
#include <cstddef>
#include <utility>

namespace emb::sensor {

// Buffered sensor: an SPSC queue decouples the producing context (ISR, via
// submit) from the consuming context (main loop, via process). process drains
// the queue through a sensor core, so conversion/filtering happens off the ISR.
// The core is any some_sensor_core -- singlephase for a single channel,
// polyphase for an aligned N-phase frame -- and the queue's element type must
// match the core's sample_type (a scalar code, or a whole frame).
template<typename Queue, typename Core>
  requires some_spsc_queue<Queue>
        && some_sensor_core<Core>
        && std::same_as<typename Queue::value_type, typename Core::sample_type>
class buffered {
public:
  using core_type = Core;
  using sample_type = typename Core::sample_type;
  using raw_type = typename Core::raw_type;
  using value_type = typename Core::value_type;
private:
  Queue queue_;
  Core core_;
public:
  template<typename... Args>
    requires std::constructible_from<Core, Args...>
  explicit buffered(Args&&... args) : core_(std::forward<Args>(args)...) {}

  Core const& core() const {
    return core_;
  }

  // Convenience forwarder for singlephase cores; absent for polyphase,
  // which is read through values() / value(phase) below.
  value_type value() const
      requires requires(Core const& c) { c.value(); } {
    return core_.value();
  }

  // Convenience forwarders for polyphase cores; absent for singlephase cores.
  auto values() const
      requires requires(Core const& c) { c.values(); } {
    return core_.values();
  }

  auto value(std::size_t phase) const
      requires requires(Core const& c) { c.value(phase); } {
    return core_.value(phase);
  }

  // Producer-side (ISR). On overflow the newest sample is dropped.
  void submit(sample_type sample) {
    auto _ = queue_.try_push(std::move(sample));
  }

  // Consumer-side (main loop). Drains the queue through the core pipeline.
  void process() {
    while (auto const raw = queue_.try_pop()) {
      core_.submit(*raw);
    }
  }
};

} // namespace emb::sensor
