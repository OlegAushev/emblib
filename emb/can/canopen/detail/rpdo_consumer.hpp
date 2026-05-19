#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <utility>

#include <emb/can.hpp>
#include <emb/delegate.hpp>

#include "../can_transport.hpp"
#include "../types.hpp"

namespace emb {
namespace canopen {
namespace detail {

class rpdo_consumer {
public:
  rpdo_consumer(can_transport& transport, node_id node);

  rpdo_consumer(rpdo_consumer const&) = delete;
  rpdo_consumer& operator=(rpdo_consumer const&) = delete;

  void set_handler(
      rpdo_num n,
      emb::delegate<void(emb::can::payload_t const&)> handler
  );

  void set_timeout(
      rpdo_num n,
      std::chrono::milliseconds timeout,
      emb::delegate<void()> on_timeout,
      std::chrono::milliseconds now
  );

  // Note: previous HW filter persists, frames matching the old COB-ID
  // will pass through but no slot will claim them in try_handle.
  void set_cob_id(rpdo_num n, emb::can::id_t custom_id);

  bool try_handle(
      emb::can::frame_t const& frame,
      std::chrono::milliseconds now,
      nmt_state state
  );

  uint8_t tick(std::chrono::milliseconds now, nmt_state state);

  void reset_timers(std::chrono::milliseconds now);

private:
  struct slot {
    emb::can::id_t cob_id = 0;

    emb::delegate<void(emb::can::payload_t const&)> handler;

    std::chrono::milliseconds timeout{0};
    std::chrono::milliseconds last_rx{0};
    emb::delegate<void()> on_timeout;
    bool timed_out = false;
  };

  can_transport& transport_;
  std::array<slot, 4> slots_;
};

} // namespace detail
} // namespace canopen
} // namespace emb
