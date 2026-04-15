#pragma once

#include <array>
#include <cstdint>

using canpayload_t = std::array<uint8_t, 8>;
using canid_t = uint32_t;

struct can_frame {
  canid_t id;
  uint8_t len;
  canpayload_t payload;
};

namespace emb {

using canpayload_t = std::array<uint8_t, 8>;
using canid_t = uint32_t;

struct canframe_t {
  canid_t id;
  uint8_t len;
  canpayload_t payload;
};

} // namespace emb
