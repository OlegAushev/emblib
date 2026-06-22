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
// match the core's sample_type (a scalar code, or a whole frame). Keeping the
// core a parameter (rather than hardcoding singlephase) lets one buffered serve
// both: a polyphase core queues whole frames, so the phases stay aligned across
// the ISR/main-loop boundary instead of drifting in N independent queues.
template<typename Queue, typename Core>
  requires some_spsc_queue<Queue>
        && some_sensor_core<Core>
        && std::same_as<typename Queue::value_type, typename Core::sample_type>
class buffered {
public:
  using core_type = Core;
  using rawdata_type = typename Core::rawdata_type;
  using sample_type = typename Core::sample_type;
  using value_type = typename Core::value_type;
private:
  Queue queue_;
  Core core_;
public:
  // Forwards to the core's constructor: (converter, filter) for singlephase, or
  // (converter, filter) / per-phase arrays for polyphase. The constraint keeps
  // this from hijacking copy/move construction.
  template<typename... Args>
    requires std::constructible_from<Core, Args...>
  explicit buffered(Args&&... args) : core_(std::forward<Args>(args)...) {}

  // Read access to the filtered outputs: core().value() for a scalar core,
  // core().values() / value(phase) for polyphase.
  Core const& core() const {
    return core_;
  }

  // Convenience forwarder for scalar cores; absent for polyphase, which is read
  // through values() / value(phase) below.
  value_type value() const
      requires requires(Core const& c) { c.value(); } {
    return core_.value();
  }

  // Convenience forwarders for polyphase cores; absent for scalar cores.
  auto values() const
      requires requires(Core const& c) { c.values(); } {
    return core_.values();
  }

  auto value(std::size_t phase) const
      requires requires(Core const& c) { c.value(phase); } {
    return core_.value(phase);
  }

  // Producer-side (ISR). On overflow the newest sample is dropped.
  void submit(sample_type data) {
    auto _ = queue_.try_push(std::move(data));
  }

  // Consumer-side (main loop). Drains the queue through the core pipeline.
  void process() {
    while (auto const raw = queue_.try_pop()) {
      core_.submit(*raw);
    }
  }
};

} // namespace emb::sensor
