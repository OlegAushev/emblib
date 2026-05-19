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

class sync_producer {
public:
  sync_producer(transport& bus) : bus_(bus) {}

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

    frame_t frame =
        {.format = format_t::standard, .id = cob_id_, .len = 0, .payload = {}};

    if (bus_.send(frame)) {
      last_tx_ = now;
    }
  }

private:
  transport& bus_;
  static constexpr id_t cob_id_ = cob_id_of<cob_type::sync>();
  std::chrono::milliseconds period_{0};
  std::chrono::milliseconds last_tx_{0};
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
