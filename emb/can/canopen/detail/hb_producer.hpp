#pragma once

#include <chrono>
#include <utility>

#include <emb/can.hpp>
#include <emb/can/bus.hpp>

#include "../types.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

template<std::uint8_t NodeId>
class hb_producer {
public:
  explicit hb_producer(transport& bus) : bus_(bus) {}

  void set_period(
      std::chrono::milliseconds period,
      std::chrono::milliseconds now
  ) {
    period_ = period;
    last_tx_ = now;
  }

  void tick(std::chrono::milliseconds now, nmt_state state) {
    if (period_ == std::chrono::milliseconds::zero()) return;
    if ((now - last_tx_) < period_) return;

    frame_t frame = {
        .format = format_t::standard,
        .id = cob_id_of<cob_type::heartbeat, NodeId>(),
        .len = 1,
        .payload = {std::to_underlying(state)}
    };

    if (bus_.send(frame)) {
      last_tx_ = now;
    }
  }

private:
  transport& bus_;
  std::chrono::milliseconds period_{0};
  std::chrono::milliseconds last_tx_{0};
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
