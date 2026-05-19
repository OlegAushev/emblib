#pragma once

#include <chrono>
#include <utility>

#include <emb/can.hpp>

#include "../can_transport.hpp"
#include "../types.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

class hb_producer {
public:
  hb_producer(can_transport& transport, node_id node)
      : transport_(transport), cob_id_(cob_id_of<cob_type::heartbeat>(node)) {}

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
        .id = cob_id_,
        .len = 1,
        .payload = {std::to_underlying(state)}
    };

    if (transport_.send(frame)) {
      last_tx_ = now;
    }
  }

private:
  can_transport& transport_;
  id_t const cob_id_;
  std::chrono::milliseconds period_{0};
  std::chrono::milliseconds last_tx_{0};
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
