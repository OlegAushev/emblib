#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

#include <emb/can.hpp>
#include <emb/can/bus.hpp>
#include <emb/delegate.hpp>

#include "../types.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

// The scoped enumeration `rpdo_refusal` defines why
// `rpdo_consumer::try_handle` did not hand a frame to a handler.
enum class rpdo_refusal : std::uint8_t {
  // No slot listens to the frame's COB-ID: the server hands it on.
  unmatched,
  // The node is not operational, and CiA 301 processes no PDO then.
  not_operational,
  // The frame is shorter than 8 bytes, the first of a run of such frames:
  // the server reports it with an EMCY.
  length_error,
  // The frame is shorter than 8 bytes and follows another such frame, whose
  // EMCY reported the run.
  repeated_length_error
};

template<std::size_t N>
class rpdo_consumer {
public:
  constexpr rpdo_consumer(transport& bus, node_id node) : bus_(bus), node_(node)
  {
  }

  rpdo_consumer(rpdo_consumer const&) = delete;
  rpdo_consumer& operator=(rpdo_consumer const&) = delete;

  template<std::size_t I, pdo_id CobId>
  constexpr void setup(rpdo_config const& cfg, std::chrono::milliseconds now)
  {
    static_assert(I >= 1 && I <= N, "RPDO index out of range");
    auto& s = slots_[I - 1];
    s.handler = cfg.handler;
    s.timeout = cfg.timeout;
    s.on_timeout = cfg.on_timeout;
    s.last_rx = now;
    s.timed_out = false;
    s.length_error = false;

    id_t id;
    if constexpr (CobId.is_custom) {
      id = CobId.value;
    }
    else {
      id = cob_id_of<cob_type::rpdo, I>(node_);
    }
    if (s.cob_id != id) {
      // A previous HW filter (if any) persists in the bus; the slot moves to
      // the new id, so old-id frames pass through but match no slot.
      bus_.add_filter(format_t::standard, id, 0x7FF);
      s.cob_id = id;
    }
  }

  // Hands `frame` to the handler of the slot that listens to its COB-ID if
  // the node is operational and the frame holds 8 bytes, as a DLC of 9 to 15
  // also does in classic CAN. A shorter frame reaches neither the handler nor
  // the slot's timeout: the first of a run of them fails with `length_error`,
  // the others with `repeated_length_error`, and a full frame, `setup` or
  // `reset_timers` ends the run. Outside operational a frame fails with
  // `not_operational` before its length counts, and a frame that no slot
  // listens to with `unmatched`.
  constexpr std::expected<void, rpdo_refusal>
  try_handle(frame_t const& frame,
             std::chrono::milliseconds now,
             nmt_state state)
  {
    for (auto& s : slots_) {
      if (s.cob_id != frame.id) continue;
      if (state != nmt_state::operational) {
        return std::unexpected(rpdo_refusal::not_operational);
      }
      if (frame.len < 8) {
        if (s.length_error) {
          return std::unexpected(rpdo_refusal::repeated_length_error);
        }
        s.length_error = true;
        return std::unexpected(rpdo_refusal::length_error);
      }
      s.last_rx = now;
      s.timed_out = false;
      s.length_error = false;
      if (s.handler) s.handler(frame.payload);
      return {};
    }
    return std::unexpected(rpdo_refusal::unmatched);
  }

  // Returns the number of slots that newly timed out on this tick.
  constexpr std::size_t tick(std::chrono::milliseconds now, nmt_state state)
  {
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

  constexpr void reset_timers(std::chrono::milliseconds now)
  {
    for (auto& s : slots_) {
      s.last_rx = now;
      s.timed_out = false;
      s.length_error = false;
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
    bool length_error = false;
  };

  transport& bus_;
  node_id node_;
  std::array<slot, N> slots_;
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
