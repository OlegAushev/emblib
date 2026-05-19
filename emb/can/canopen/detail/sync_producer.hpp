#pragma once

#include <chrono>
#include <utility>

#include <emb/can.hpp>

#include "../can_transport.hpp"
#include "../canopen_def.hpp"

namespace emb {
namespace canopen {
namespace detail {

class sync_producer {
public:
  sync_producer(can_transport& transport) : transport_(transport) {}

  void set_period(
      std::chrono::milliseconds period,
      std::chrono::milliseconds now
  ) {
    period_ = period;
    last_tx_ = now;
  }

  void tick(std::chrono::milliseconds now) {
    if (period_ == std::chrono::milliseconds::zero()) return;
    if ((now - last_tx_) < period_) return;

    emb::can::frame_t frame = {
        .format = emb::can::format_t::standard,
        .id = cob_id_,
        .len = 0,
        .payload = {}
    };

    if (transport_.send(frame)) {
      last_tx_ = now;
    }
  }

private:
  can_transport& transport_;
  static constexpr emb::can::id_t cob_id_ = cob_id_of<cob_type::sync>();
  std::chrono::milliseconds period_{0};
  std::chrono::milliseconds last_tx_{0};
};

} // namespace detail
} // namespace canopen
} // namespace emb
