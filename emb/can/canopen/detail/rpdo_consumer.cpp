#include "rpdo_consumer.hpp"

#include <utility>

namespace emb {
namespace canopen {
namespace detail {

rpdo_consumer::rpdo_consumer(can_transport& transport, node_id id)
    : transport_(transport) {
  slots_[0].cob_id = cob_id_of<cob_type::rpdo1>(id);
  slots_[1].cob_id = cob_id_of<cob_type::rpdo2>(id);
  slots_[2].cob_id = cob_id_of<cob_type::rpdo3>(id);
  slots_[3].cob_id = cob_id_of<cob_type::rpdo4>(id);
  for (auto& s : slots_) {
    transport_.add_filter(s.cob_id, 0x7FF);
  }
}

void rpdo_consumer::set_handler(
    rpdo_num n,
    emb::delegate<void(emb::can::payload_t const&)> handler
) {
  slots_[std::to_underlying(n)].handler = handler;
}

void rpdo_consumer::set_timeout(
    rpdo_num n,
    std::chrono::milliseconds timeout,
    emb::delegate<void()> on_timeout,
    std::chrono::milliseconds now
) {
  auto& s = slots_[std::to_underlying(n)];
  s.timeout = timeout;
  s.on_timeout = on_timeout;
  s.last_rx = now;
  s.timed_out = false;
}

void rpdo_consumer::set_cob_id(rpdo_num n, emb::can::id_t custom_id) {
  auto& s = slots_[std::to_underlying(n)];
  if (s.cob_id == custom_id) return;

  // Old HW filter persists in the transport; library-level slot moves to
  // the new id, so old-id frames pass through but match nothing.
  transport_.add_filter(custom_id, 0x7FF);
  s.cob_id = custom_id;
}

bool rpdo_consumer::try_handle(
    emb::can::frame_t const& frame,
    std::chrono::milliseconds now,
    nmt_state state
) {
  for (auto& s : slots_) {
    if (frame.id != s.cob_id) continue;
    if (state != nmt_state::operational) return true;
    s.last_rx = now;
    s.timed_out = false;
    if (s.handler) s.handler(frame.payload);
    return true;
  }
  return false;
}

uint8_t rpdo_consumer::tick(std::chrono::milliseconds now, nmt_state state) {
  if (state != nmt_state::operational) return 0;

  uint8_t just_timed_out = 0;
  for (size_t i = 0; i < slots_.size(); ++i) {
    auto& s = slots_[i];
    if (s.timeout == std::chrono::milliseconds::zero() || s.timed_out) {
      continue;
    }
    if ((now - s.last_rx) >= s.timeout) {
      s.timed_out = true;
      if (s.on_timeout) s.on_timeout();
      just_timed_out |= static_cast<uint8_t>(1u << i);
    }
  }
  return just_timed_out;
}

void rpdo_consumer::reset_timers(std::chrono::milliseconds now) {
  for (auto& s : slots_) {
    s.last_rx = now;
    s.timed_out = false;
  }
}

} // namespace detail
} // namespace canopen
} // namespace emb
