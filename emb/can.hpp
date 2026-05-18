#pragma once

#include <array>
#include <cstdint>

namespace emb {

using canpayload_t = std::array<uint8_t, 8>;
using canid_t = uint32_t;
enum class canformat_t : uint8_t { standard, extended };

struct canframe_t {
  canformat_t format;
  canid_t id;
  uint8_t len;
  canpayload_t payload;
};

} // namespace emb
