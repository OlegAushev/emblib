#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include <emb/can.hpp>

#include "../types.hpp"

namespace emb {
namespace canopen {
namespace detail {

class nmt_slave {
public:
  explicit nmt_slave(node_id node) : node_(node) {}

  static constexpr emb::can::id_t cob_id() {
    return cob_id_;
  }

  bool match(emb::can::frame_t const& frame) const {
    return frame.id == cob_id();
  }

  nmt_state state() const {
    return state_;
  }

  void set_state(nmt_state s) {
    state_ = s;
  }

  std::optional<nmt_command> decode(emb::can::frame_t const& frame) const {
    if (frame.len < 2) return std::nullopt;
    uint8_t cs = frame.payload[0];
    uint8_t target = frame.payload[1];
    if (target != 0 && target != node_.get()) return std::nullopt;

    switch (cs) {
    case 0x01: return nmt_command::start;
    case 0x02: return nmt_command::stop;
    case 0x80: return nmt_command::enter_pre_operational;
    case 0x81: return nmt_command::reset_node;
    case 0x82: return nmt_command::reset_communication;
    default: return std::nullopt;
    }
  }

private:
  node_id const node_;
  static constexpr emb::can::id_t cob_id_ = cob_id_of<cob_type::nmt>();
  nmt_state state_ = nmt_state::initializing;
};

} // namespace detail
} // namespace canopen
} // namespace emb
