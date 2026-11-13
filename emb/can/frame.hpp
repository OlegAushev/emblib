#pragma once

#include <array>
#include <cstdint>

namespace emb {
namespace can {

using payload_t = std::array<std::uint8_t, 8>;
using id_t = std::uint32_t;
enum class format_t : std::uint8_t { standard, extended };

struct frame_t {
  format_t format;
  id_t id;
  std::uint8_t len;
  payload_t payload;
};

} // namespace can
} // namespace emb
