#pragma once

#include <array>
#include <chrono>
#include <cstddef>

#include <emb/can.hpp>
#include <emb/can/bus.hpp>
#include <emb/delegate.hpp>

#include "../types.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

template<std::uint8_t NodeId, std::size_t N>
class tpdo_producer {
public:
  explicit tpdo_producer(transport& bus) : bus_(bus) {}

  template<std::size_t I, pdo_id CobId>
  void setup(tpdo_config const& cfg, std::chrono::milliseconds now) {
    static_assert(I >= 1 && I <= N, "TPDO index out of range");
    auto& s = slots_[I - 1];
    s.provider = cfg.provider;
    s.period = cfg.period;
    s.last_tx = now;
    if constexpr (CobId.is_custom) {
      s.cob_id = CobId.value;
    } else {
      s.cob_id = cob_id_of<cob_type::tpdo, I, NodeId>();
    }
  }

  void tick(std::chrono::milliseconds now, nmt_state state) {
    if (state != nmt_state::operational) return;

    for (auto& s : slots_) {
      if (!s.provider) continue;
      if (s.period == std::chrono::milliseconds::zero()) continue;
      if ((now - s.last_tx) < s.period) continue;

      frame_t frame = {
          .format = format_t::standard,
          .id = s.cob_id,
          .len = 8,
          .payload = s.provider()
      };

      if (bus_.send(frame)) {
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

  transport& bus_;
  std::array<slot, N> slots_;
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
