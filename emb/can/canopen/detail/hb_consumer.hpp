#pragma once

#include <chrono>
#include <cstdint>

#include <emb/can.hpp>
#include <emb/can/bus.hpp>
#include <emb/container/inplace_vector.hpp>
#include <emb/delegate.hpp>

#include "../types.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

class hb_consumer {
public:
  static constexpr std::size_t capacity = 8;

  explicit hb_consumer(transport& bus) : bus_(bus) {}

  hb_consumer(hb_consumer const&) = delete;
  hb_consumer& operator=(hb_consumer const&) = delete;

  template<std::uint8_t Remote>
  bool watch(
      std::chrono::milliseconds timeout,
      emb::delegate<void(std::uint8_t)> on_lost,
      std::chrono::milliseconds now
  ) {
    static_assert(valid_node_id<Remote>, "node id must be in [1, 127]");
    constexpr id_t cob_id = cob_id_of<cob_type::heartbeat, Remote>();

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
             .remote = Remote,
             .timeout = timeout,
             .last_rx = now,
             .lost = false,
             .on_lost = on_lost}
        )) {
      return false;
    }
    bus_.add_filter(format_t::standard, cob_id, 0x7FF);
    return true;
  }

  // Called from server::dispatch_rx for each incoming frame; updates the
  // matching watch's last_rx and clears `lost`. Returns true on match.
  bool try_handle(frame_t const& frame, std::chrono::milliseconds now);

  // Checks timeouts, fires on_lost once per loss event.
  void tick(std::chrono::milliseconds now);

private:
  struct watch_slot {
    id_t cob_id = 0;
    std::uint8_t remote = 0;

    std::chrono::milliseconds timeout{0};
    std::chrono::milliseconds last_rx{0};
    bool lost = false;

    emb::delegate<void(std::uint8_t)> on_lost;
  };

  transport& bus_;
  emb::inplace_vector<watch_slot, capacity> watches_;
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
