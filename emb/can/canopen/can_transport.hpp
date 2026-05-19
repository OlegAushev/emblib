#pragma once

#include <cstdint>

#include <emb/can.hpp>
#include <emb/delegate.hpp>

namespace emb {
namespace canopen {

class can_transport {
public:
  virtual ~can_transport() = default;

  virtual bool send(emb::can::frame_t const& frame) = 0;

  // Register an RX subscriber. Multiple subscribers may register; each
  // accepted frame is delivered to all of them in registration order.
  virtual void
  subscribe(emb::delegate<void(emb::can::frame_t const&)> handler) = 0;

  // Configure a HW acceptance filter. There is no removal — embedded
  // filters are set up at boot and persist for the device's lifetime.
  virtual void add_filter(emb::can::id_t id, emb::can::id_t mask) = 0;
};

} // namespace canopen
} // namespace emb
