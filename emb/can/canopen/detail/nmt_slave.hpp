#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include <emb/can.hpp>

#include "../types.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

class nmt_slave {
public:
  explicit constexpr nmt_slave(node_id node) : node_(node) {}

  static constexpr id_t cob_id()
  {
    return cob_id_;
  }

  constexpr bool match(frame_t const& frame) const
  {
    return frame.id == cob_id();
  }

  constexpr nmt_state state() const
  {
    return state_;
  }

  constexpr void set_state(nmt_state s)
  {
    state_ = s;
  }

  constexpr std::optional<nmt_command> decode(frame_t const& frame) const
  {
    if (frame.len < 2) return std::nullopt;
    std::uint8_t cs = frame.payload[0];
    std::uint8_t target = frame.payload[1];
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
  static constexpr id_t cob_id_ = cob_id_of<cob_type::nmt>();
  node_id node_;
  nmt_state state_ = nmt_state::initializing;
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
