#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>

#include <emb/can.hpp>
#include <emb/can/bus.hpp>
#include <emb/delegate.hpp>

#include "../types.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

template<std::uint8_t NodeId, std::size_t N>
class rpdo_consumer {
public:
  explicit rpdo_consumer(transport& bus) : bus_(bus) {}

  rpdo_consumer(rpdo_consumer const&) = delete;
  rpdo_consumer& operator=(rpdo_consumer const&) = delete;

  template<std::size_t I, pdo_id CobId>
  void setup(rpdo_config const& cfg, std::chrono::milliseconds now) {
    static_assert(I >= 1 && I <= N, "RPDO index out of range");
    auto& s = slots_[I - 1];
    s.handler = cfg.handler;
    s.timeout = cfg.timeout;
    s.on_timeout = cfg.on_timeout;
    s.last_rx = now;
    s.timed_out = false;

    id_t id;
    if constexpr (CobId.is_custom) {
      id = CobId.value;
    } else {
      id = cob_id_of<cob_type::rpdo, I, NodeId>();
    }
    if (s.cob_id != id) {
      // A previous HW filter (if any) persists in the bus; the slot moves to
      // the new id, so old-id frames pass through but match no slot.
      bus_.add_filter(format_t::standard, id, 0x7FF);
      s.cob_id = id;
    }
  }

  bool try_handle(
      frame_t const& frame,
      std::chrono::milliseconds now,
      nmt_state state
  ) {
    for (auto& s : slots_) {
      if (s.cob_id != frame.id) continue;
      if (state != nmt_state::operational) return true;
      s.last_rx = now;
      s.timed_out = false;
      if (s.handler) s.handler(frame.payload);
      return true;
    }
    return false;
  }

  // Returns the number of slots that newly timed out on this tick.
  std::size_t tick(std::chrono::milliseconds now, nmt_state state) {
    if (state != nmt_state::operational) return 0;

    std::size_t just_timed_out = 0;
    for (auto& s : slots_) {
      if (s.timeout == std::chrono::milliseconds::zero() || s.timed_out) {
        continue;
      }
      if ((now - s.last_rx) >= s.timeout) {
        s.timed_out = true;
        if (s.on_timeout) s.on_timeout();
        ++just_timed_out;
      }
    }
    return just_timed_out;
  }

  void reset_timers(std::chrono::milliseconds now) {
    for (auto& s : slots_) {
      s.last_rx = now;
      s.timed_out = false;
    }
  }

private:
  struct slot {
    std::optional<id_t> cob_id;

    emb::delegate<void(payload_t const&)> handler;

    std::chrono::milliseconds timeout{0};
    std::chrono::milliseconds last_rx{0};
    emb::delegate<void()> on_timeout;
    bool timed_out = false;
  };

  transport& bus_;
  std::array<slot, N> slots_;
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
