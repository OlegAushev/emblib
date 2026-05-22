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

  bool watch(
      node_id remote,
      std::chrono::milliseconds timeout,
      emb::delegate<void(node_id)> on_lost,
      std::chrono::milliseconds now
  );

  // Called from server::dispatch_rx for each incoming frame; updates the
  // matching watch's last_rx and clears `lost`. Returns true on match.
  bool try_handle(frame_t const& frame, std::chrono::milliseconds now);

  // Checks timeouts, fires on_lost once per loss event.
  void tick(std::chrono::milliseconds now);

private:
  struct watch_slot {
    id_t cob_id = 0;
    node_id remote = node_id::make(1).value();

    std::chrono::milliseconds timeout{0};
    std::chrono::milliseconds last_rx{0};
    bool lost = false;

    emb::delegate<void(node_id)> on_lost;
  };

  transport& bus_;
  emb::inplace_vector<watch_slot, capacity> watches_;
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
