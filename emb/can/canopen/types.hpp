#pragma once

#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <emb/can.hpp>
#include <emb/delegate.hpp>

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

template<std::uint8_t Id>
concept valid_node_id = Id >= 1 && Id <= 127;

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
  tpdo,
  rpdo,
  tsdo,
  rsdo,
  heartbeat
};

constexpr std::size_t cob_type_count = 9;

constexpr std::array<id_t, cob_type_count> cob_function_codes = {
    0x000, // nmt
    0x080, // sync
    0x080, // emcy
    0x100, // time
    0x180, // tpdo (base)
    0x200, // rpdo (base)
    0x580, // tsdo
    0x600, // rsdo
    0x700  // heartbeat
};

template<cob_type C>
concept broadcast_cob =
    C == cob_type::nmt || C == cob_type::sync || C == cob_type::time;

template<cob_type C>
concept pdo_cob = C == cob_type::tpdo || C == cob_type::rpdo;

template<cob_type C>
concept peer_cob = !broadcast_cob<C> && !pdo_cob<C>;

template<cob_type Service>
  requires broadcast_cob<Service>
constexpr id_t cob_id_of() {
  return cob_function_codes[std::to_underlying(Service)];
}

template<cob_type Service, std::uint8_t NodeId>
  requires peer_cob<Service>
constexpr id_t cob_id_of() {
  static_assert(valid_node_id<NodeId>, "node id must be in [1, 127]");
  return cob_function_codes[std::to_underlying(Service)] + NodeId;
}

template<cob_type Pdo, std::size_t I, std::uint8_t NodeId>
  requires pdo_cob<Pdo>
constexpr id_t cob_id_of() {
  static_assert(I >= 1 && I <= 4, "predefined COB-ID exists only for PDO 1..4");
  static_assert(valid_node_id<NodeId>, "node id must be in [1, 127]");
  return cob_function_codes[std::to_underlying(Pdo)]
         + static_cast<id_t>(0x100 * (I - 1)) + NodeId;
}

struct pdo_id {
  id_t value = 0;
  bool is_custom = false;

  constexpr pdo_id() = default;

  static constexpr pdo_id predefined() {
    return {};
  }

  static constexpr pdo_id custom(id_t v) {
    return pdo_id(v, true);
  }

private:
  constexpr pdo_id(id_t v, bool c) : value(v), is_custom(c) {}
};

struct tpdo_config {
  emb::delegate<payload_t()> provider;
  std::chrono::milliseconds period{0}; // 0 = do not transmit
};

struct rpdo_config {
  emb::delegate<void(payload_t const&)> handler;
  std::chrono::milliseconds timeout{0}; // 0 = liveness monitoring off
  emb::delegate<void()> on_timeout;
};

} // namespace canopen
} // namespace can
} // namespace emb
