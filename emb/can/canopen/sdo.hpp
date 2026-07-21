#pragma once

#include <array>
#include <cstdint>
#include <utility>

#include <emb/can.hpp>

namespace emb {
namespace can {
namespace canopen {

namespace sdo_cs_codes {
constexpr std::uint32_t client_init_write = 1;
constexpr std::uint32_t server_init_write = 3;
constexpr std::uint32_t client_init_read = 2;
constexpr std::uint32_t server_init_read = 2;

constexpr std::uint32_t abort = 4;

constexpr std::uint32_t client_block_write = 6;
constexpr std::uint32_t server_block_write = 5;

constexpr std::uint32_t client_block_read = 5;
constexpr std::uint32_t server_block_read = 6;
} // namespace sdo_cs_codes

inline std::uint32_t get_cs_code(frame_t const& frame) {
  return (frame.payload[0] >> 5) & 0x07;
}

// On-wire 4-byte data field of an expedited SDO frame.
using expedited_sdo_data = std::array<std::uint8_t, 4>;

struct expedited_sdo {
  std::uint32_t data_size_indicated : 1;
  std::uint32_t expedited_transfer : 1;
  std::uint32_t data_empty_bytes : 2;
  std::uint32_t _reserved : 1;
  std::uint32_t cs : 3;
  std::uint32_t index : 16;
  std::uint32_t subindex : 8;
  expedited_sdo_data data;

  expedited_sdo()
      : data_size_indicated(0),
        expedited_transfer(0),
        data_empty_bytes(0),
        _reserved(0),
        cs(0),
        index(0),
        subindex(0),
        data{} {}
};

enum class sdo_abort_code : std::uint32_t {
  invalid_cs = 0x05040001,
  unsupported_access = 0x06010000,
  read_from_write_only = 0x06010001,
  write_to_read_only = 0x06010002,
  object_not_found = 0x06020000,
  hardware_error = 0x06060000,
  data_type_mismatch = 0x06070010,
  value_range_exceeded = 0x06090030,
  value_too_high = 0x06090031,
  value_too_low = 0x06090032,
  general_error = 0x08000000,
  data_store_error = 0x08000020,
  local_control_error = 0x08000021,
  state_error = 0x08000022,
  no_data_available = 0x08000024
};

struct abort_sdo {
  std::uint32_t _reserved : 5;
  std::uint32_t cs : 3;
  std::uint32_t index : 16;
  std::uint32_t subindex : 8;
  std::uint32_t error_code;
  abort_sdo() = default;

  abort_sdo(
      std::uint16_t index_,
      std::uint8_t subindex_,
      sdo_abort_code error_code_
  )
      : _reserved(0),
        cs(sdo_cs_codes::abort),
        index(index_),
        subindex(subindex_),
        error_code(std::to_underlying(error_code_)) {}

  bool valid() const {
    return cs == sdo_cs_codes::abort;
  }
};

} // namespace canopen
} // namespace can
} // namespace emb
