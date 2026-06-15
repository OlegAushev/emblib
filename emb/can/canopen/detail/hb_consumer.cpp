#include "hb_consumer.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

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
