#pragma once

#include <emb/can.hpp>
#include <emb/delegate.hpp>

namespace emb {
namespace can {

class transport {
public:
  virtual ~transport() = default;

  virtual bool send(frame_t const& frame) = 0;

  // Register an RX subscriber. Multiple subscribers may register; each
  // accepted frame is delivered to all of them in registration order.
  virtual void subscribe(emb::delegate<void(frame_t const&)> handler) = 0;

  // Configure a HW acceptance filter. Format is explicit — drivers must
  // route standard (11-bit) vs extended (29-bit) frames to appropriate
  // hardware resources (e.g. STM32 filter banks in 16-bit vs 32-bit mode).
  virtual void add_filter(format_t format, id_t id, id_t mask) = 0;
};

} // namespace can
} // namespace emb
