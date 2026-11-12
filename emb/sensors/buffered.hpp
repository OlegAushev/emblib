#pragma once

#include <emb/sensors/basic.hpp>
#include <emb/sensors/concepts.hpp>

#include <concepts>
#include <cstddef>
#include <utility>

namespace emb::sensors {

// Buffered sensor: an SPSC queue decouples the producing context
// from the consuming context. update() drains the queue through a sensor core.
// The core is any immediate sensor -- basic for one channel,
// multichannel for an aligned N-channel frame -- and the queue's element
// type must match the core's input_type (a sample, or a whole frame).
// buffered takes an immediate sensor and makes it deferred, so it cannot
// nest: the outer update() would never drive the inner one.
template<typename Queue, typename Core>
  requires some_spsc_queue<Queue>
        && immediate_sensor<Core>
        && std::same_as<typename Queue::value_type, typename Core::input_type>
class buffered {
public:
  using core_type = Core;
  using input_type = typename Core::input_type;
  using output_type = typename Core::output_type;
  using sensor_category = deferred_tag;
private:
  Queue queue_;
  Core core_;
public:
  template<typename... Args>
    requires std::constructible_from<Core, Args...>
  explicit buffered(Args&&... args) : core_(std::forward<Args>(args)...)
  {
  }

  Core const& core() const
  {
    return core_;
  }

  Core& core()
  {
    return core_;
  }

  // Convenience forwarder for basic cores; absent for multichannel,
  // which is read through outputs() / output(channel) below.
  output_type output() const
    requires requires(Core const& c) { c.output(); }
  {
    return core_.output();
  }

  // Convenience forwarders for multichannel cores; absent for basic
  // cores.
  auto outputs() const
    requires requires(Core const& c) { c.outputs(); }
  {
    return core_.outputs();
  }

  auto output(std::size_t channel) const
    requires requires(Core const& c) { c.output(channel); }
  {
    return core_.output(channel);
  }

  // Producer-side (ISR). On overflow the newest sample is dropped.
  void push(input_type sample)
  {
    auto _ = queue_.try_push(std::move(sample));
  }

  // Consumer-side (main loop). Drains the queue through the core pipeline.
  void update()
  {
    while (auto const sample = queue_.try_pop()) {
      core_.update(*sample);
    }
  }
};

} // namespace emb::sensors
