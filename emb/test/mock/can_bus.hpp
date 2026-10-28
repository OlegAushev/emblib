#pragma once

#include <emb/can/bus.hpp>

#include <cstddef>

#include <emb/can.hpp>
#include <emb/delegate.hpp>

namespace emb {
namespace test {

// A CAN bus for tests. Constexpr throughout, so the parts of a CANopen server
// run inside static_assert and cost nothing at run time. `send` takes every
// frame, as a driver takes it into its transmit queue, and keeps the last,
// unless the test fills the queue; the bus receives nothing, and `subscribe`
// and `add_filter` do nothing.
class can_bus : public emb::can::transport {
public:
  constexpr bool send(emb::can::frame_t const& frame) override
  {
    if (queue_full_) {
      ++refused;
      return false;
    }
    ++taken;
    last_ = frame;
    return true;
  }

  constexpr void
  subscribe(emb::delegate<void(emb::can::frame_t const&)>) override
  {
  }

  constexpr void add_filter(emb::can::format_t,
                            emb::can::id_t,
                            emb::can::id_t) override
  {
  }

  // -- Test controls --

  // A driver whose transmit queue is full, as it fills while no node
  // acknowledges the frames: `send` refuses every frame until the queue is
  // set free again.
  constexpr void set_queue_full(bool on)
  {
    queue_full_ = on;
  }

  // The last frame that `send` took.
  constexpr emb::can::frame_t const& last() const
  {
    return last_;
  }

  // The frames that `send` took and those it refused.
  std::size_t taken = 0;
  std::size_t refused = 0;

private:
  bool queue_full_ = false;
  emb::can::frame_t last_{};
};

} // namespace test
} // namespace emb
