#include "hb_consumer.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

bool hb_consumer::watch(
    node_id remote,
    std::chrono::milliseconds timeout,
    emb::delegate<void(node_id)> on_lost,
    std::chrono::milliseconds now
) {
  id_t cob_id = cob_id_of<cob_type::heartbeat>(remote);

  // update existing watch on this cob_id
  for (auto& w : watches_) {
    if (w.cob_id != cob_id) continue;
    w.timeout = timeout;
    w.on_lost = on_lost;
    w.last_rx = now;
    w.lost = false;
    return true;
  }

  // register new
  if (!watches_.try_push_back(
          {.cob_id = cob_id,
           .remote = remote,
           .timeout = timeout,
           .last_rx = now,
           .lost = false,
           .on_lost = on_lost}
      )) {
    return false;
  }
  transport_.add_filter(cob_id, 0x7FF);
  return true;
}

bool hb_consumer::try_handle(
    frame_t const& frame,
    std::chrono::milliseconds now
) {
  for (auto& w : watches_) {
    if (w.cob_id != frame.id) continue;
    w.last_rx = now;
    w.lost = false;
    return true;
  }
  return false;
}

void hb_consumer::tick(std::chrono::milliseconds now) {
  for (auto& w : watches_) {
    if (w.timeout == std::chrono::milliseconds::zero() || w.lost) continue;
    if ((now - w.last_rx) >= w.timeout) {
      w.lost = true;
      if (w.on_lost) w.on_lost(w.remote);
    }
  }
}

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
