#pragma once

#include <array>
#include <cstdint>

#include <emb/can.hpp>
#include <emb/can/bus.hpp>

#include "../types.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

class emcy_producer {
public:
  emcy_producer(transport& bus, node_id node)
      : bus_(bus), cob_id_(cob_id_of<cob_type::emcy>(node)) {}

  bool emit(
      uint16_t error_code,
      uint8_t error_register,
      std::array<uint8_t, 5> manufacturer = {}
  ) {
    frame_t frame = {
        .format = format_t::standard,
        .id = cob_id_,
        .len = 8,
        .payload = {
            static_cast<uint8_t>(error_code & 0xFF),
            static_cast<uint8_t>((error_code >> 8) & 0xFF),
            error_register
        }
    };

    for (size_t i = 0; i < 5; ++i) {
      frame.payload[3 + i] = manufacturer[i];
    }

    return bus_.send(frame);
  }

private:
  transport& bus_;
  id_t cob_id_;
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
