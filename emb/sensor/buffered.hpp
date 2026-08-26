#pragma once

#include <emb/sensor/concepts.hpp>
#include <emb/sensor/singlechannel.hpp>

#include <concepts>
#include <cstddef>
#include <utility>

namespace emb::sensor {

// Buffered sensor: an SPSC queue decouples the producing context
// from the consuming context. process drains the queue through a sensor core.
// The core is any immediate sensor -- singlechannel for one channel,
// multichannel for an aligned N-channel frame -- and the queue's element
// type must match the core's sample_type (a scalar value, or a whole frame).
// buffered takes an immediate sensor and makes it deferred, so it cannot
// nest: the outer process() would never drive the inner one.
template<typename Queue, typename Core>
  requires some_spsc_queue<Queue>
        && some_immediate_sensor<Core>
        && std::same_as<typename Queue::value_type, typename Core::sample_type>
class buffered {
public:
  using core_type = Core;
  using sample_type = typename Core::sample_type;
  using raw_type = typename Core::raw_type;
  using value_type = typename Core::value_type;
  using sensor_category = deferred_tag;
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

  // Convenience forwarder for singlechannel cores; absent for multichannel,
  // which is read through values() / value(channel) below.
  value_type value() const
      requires requires(Core const& c) { c.value(); } {
    return core_.value();
  }

  // Convenience forwarders for multichannel cores; absent for singlechannel
  // cores.
  auto values() const
      requires requires(Core const& c) { c.values(); } {
    return core_.values();
  }

  auto value(std::size_t channel) const
      requires requires(Core const& c) { c.value(channel); } {
    return core_.value(channel);
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
