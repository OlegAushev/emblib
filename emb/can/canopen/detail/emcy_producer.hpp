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

template<std::uint8_t NodeId>
class emcy_producer {
public:
  explicit emcy_producer(transport& bus) : bus_(bus) {}

  bool emit(
      std::uint16_t error_code,
      std::uint8_t error_register,
      std::array<std::uint8_t, 5> manufacturer = {}
  ) {
    frame_t frame = {
        .format = format_t::standard,
        .id = cob_id_of<cob_type::emcy, NodeId>(),
        .len = 8,
        .payload = {
            static_cast<std::uint8_t>(error_code & 0xFF),
            static_cast<std::uint8_t>((error_code >> 8) & 0xFF),
            error_register
        }
    };

    for (auto i = 0uz; i < 5; ++i) {
      frame.payload[3 + i] = manufacturer[i];
    }

    return bus_.send(frame);
  }

private:
  transport& bus_;
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
