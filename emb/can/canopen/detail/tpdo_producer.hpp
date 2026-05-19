#pragma once

#include <array>
#include <chrono>
#include <utility>

#include <emb/can.hpp>
#include <emb/delegate.hpp>

#include "../can_transport.hpp"
#include "../types.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

class tpdo_producer {
public:
  tpdo_producer(can_transport& transport, node_id node)
      : transport_(transport) {
    slots_[0].cob_id = cob_id_of<cob_type::tpdo1>(node);
    slots_[1].cob_id = cob_id_of<cob_type::tpdo2>(node);
    slots_[2].cob_id = cob_id_of<cob_type::tpdo3>(node);
    slots_[3].cob_id = cob_id_of<cob_type::tpdo4>(node);
  }

  void set_provider(tpdo_num n, emb::delegate<payload_t()> provider) {
    slots_[std::to_underlying(n)].provider = provider;
  }

  void set_period(
      tpdo_num n,
      std::chrono::milliseconds period,
      std::chrono::milliseconds now
  ) {
    auto& s = slots_[std::to_underlying(n)];
    s.period = period;
    s.last_tx = now;
  }

  void tick(std::chrono::milliseconds now, nmt_state state) {
    if (state != nmt_state::operational) return;

    for (auto& s : slots_) {
      if (s.period == std::chrono::milliseconds::zero() || !s.provider) {
        continue;
      }
      if ((now - s.last_tx) < s.period) continue;

      frame_t frame = {
          .format = format_t::standard,
          .id = s.cob_id,
          .len = 8,
          .payload = s.provider()
      };

      if (transport_.send(frame)) {
        s.last_tx = now;
      }
    }
  }

private:
  struct slot {
    id_t cob_id = 0;
    emb::delegate<payload_t()> provider;
    std::chrono::milliseconds period{0};
    std::chrono::milliseconds last_tx{0};
  };

  can_transport& transport_;
  std::array<slot, 4> slots_;
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
