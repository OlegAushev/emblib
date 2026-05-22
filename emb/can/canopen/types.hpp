#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

#include <emb/can.hpp>

namespace emb {
namespace can {
namespace canopen {

template<typename T>
  requires std::is_trivially_copyable_v<T> && (sizeof(T) == sizeof(payload_t))
constexpr payload_t to_payload(T const& message) {
  return std::bit_cast<payload_t>(message);
}

template<typename T>
  requires std::is_trivially_copyable_v<T> && (sizeof(T) == sizeof(payload_t))
constexpr T from_payload(payload_t const& payload) {
  return std::bit_cast<T>(payload);
}

class node_id {
private:
  std::uint8_t id_;
  constexpr explicit node_id(std::uint8_t id) : id_(id) {}
public:
  static constexpr std::optional<node_id> make(std::uint8_t id) {
    if (id < 1 || id > 127) return std::nullopt;
    return node_id(id);
  }
  constexpr std::uint8_t get() const {
    return id_;
  }
};

enum class nmt_state : std::uint8_t {
  initializing = 0x00,
  stopped = 0x04,
  operational = 0x05,
  pre_operational = 0x7F
};

enum class nmt_command : std::uint8_t {
  start,                 // 0x01
  stop,                  // 0x02
  enter_pre_operational, // 0x80
  reset_node,            // 0x81
  reset_communication,   // 0x82
};

enum class cob_type : std::uint8_t {
  nmt,
  sync,
  emcy,
  time,
  tpdo1,
  rpdo1,
  tpdo2,
  rpdo2,
  tpdo3,
  rpdo3,
  tpdo4,
  rpdo4,
  tsdo,
  rsdo,
  heartbeat
};

constexpr std::size_t cob_type_count = 15;

constexpr std::array<id_t, cob_type_count> cob_function_codes = {
    0x000, // nmt
    0x080, // sync
    0x080, // emcy
    0x100, // time
    0x180, // tpdo1
    0x200, // rpdo1
    0x280, // tpdo2
    0x300, // rpdo2
    0x380, // tpdo3
    0x400, // rpdo3
    0x480, // tpdo4
    0x500, // rpdo4
    0x580, // tsdo
    0x600, // rsdo
    0x700  // heartbeat
};

template<cob_type Cob>
constexpr bool is_broadcast_cob = (Cob == cob_type::nmt)
                               || (Cob == cob_type::sync)
                               || (Cob == cob_type::time);

template<cob_type Cob>
  requires is_broadcast_cob<Cob>
constexpr id_t cob_id_of() {
  return cob_function_codes[std::to_underlying(Cob)];
}

template<cob_type Cob>
  requires(!is_broadcast_cob<Cob>)
constexpr id_t cob_id_of(node_id id) {
  return cob_function_codes[std::to_underlying(Cob)] + id.get();
}

enum class tpdo_num : std::uint8_t { _1, _2, _3, _4 };
enum class rpdo_num : std::uint8_t { _1, _2, _3, _4 };

constexpr cob_type to_cob(tpdo_num n) {
  constexpr std::array<cob_type, 4> map =
      {cob_type::tpdo1, cob_type::tpdo2, cob_type::tpdo3, cob_type::tpdo4};
  return map[std::to_underlying(n)];
}

constexpr cob_type to_cob(rpdo_num n) {
  constexpr std::array<cob_type, 4> map =
      {cob_type::rpdo1, cob_type::rpdo2, cob_type::rpdo3, cob_type::rpdo4};
  return map[std::to_underlying(n)];
}

} // namespace canopen
} // namespace can
} // namespace emb
