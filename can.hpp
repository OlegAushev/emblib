#pragma once

#if __cplusplus >= 201100

#include <array>
#include <cstdint>

using canpayload_t = std::array<uint8_t, 8>;
using canid_t = uint32_t;

struct can_frame {
  canid_t id;
  uint8_t len;
  canpayload_t payload;
};

#else

#include <emblib/array.hpp>
#include <stdint.h>

typedef emb::array<uint8_t, 8> canpayload_t;
typedef uint32_t canid_t;

struct can_frame {
  canid_t id;
  uint8_t len;
  canpayload_t payload;
};

#endif
