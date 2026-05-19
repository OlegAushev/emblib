#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include <emb/can.hpp>
#include <emb/units.hpp>

namespace emb {
namespace canopen {

template<typename T>
  requires std::is_trivially_copyable_v<T>
        && (sizeof(T) == sizeof(emb::can::payload_t))
constexpr emb::can::payload_t to_payload(T const& message) {
  return std::bit_cast<emb::can::payload_t>(message);
}

template<typename T>
  requires std::is_trivially_copyable_v<T>
        && (sizeof(T) == sizeof(emb::can::payload_t))
constexpr T from_payload(emb::can::payload_t const& payload) {
  return std::bit_cast<T>(payload);
}

class node_id {
private:
  uint8_t id_;
  constexpr explicit node_id(uint8_t id) : id_(id) {}
public:
  static constexpr std::optional<node_id> make(uint8_t id) {
    if (id < 1 || id > 127) return std::nullopt;
    return node_id(id);
  }
  constexpr uint8_t get() const {
    return id_;
  }
};

enum class nmt_state : uint8_t {
  initializing = 0x00,
  stopped = 0x04,
  operational = 0x05,
  pre_operational = 0x7F
};

enum class nmt_command : uint8_t {
  start,                 // 0x01
  stop,                  // 0x02
  enter_pre_operational, // 0x80
  reset_node,            // 0x81
  reset_communication,   // 0x82
};

enum class cob_type : uint8_t {
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

constexpr size_t cob_type_count = 15;

constexpr std::array<emb::can::id_t, cob_type_count> cob_function_codes = {
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
constexpr emb::can::id_t cob_id_of() {
  return cob_function_codes[std::to_underlying(Cob)];
}

template<cob_type Cob>
  requires(!is_broadcast_cob<Cob>)
constexpr emb::can::id_t cob_id_of(node_id id) {
  return cob_function_codes[std::to_underlying(Cob)] + id.get();
}

enum class tpdo_num : uint8_t { _1, _2, _3, _4 };
enum class rpdo_num : uint8_t { _1, _2, _3, _4 };

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

namespace sdo_cs_codes {
constexpr uint32_t client_init_write = 1;
constexpr uint32_t server_init_write = 3;
constexpr uint32_t client_init_read = 2;
constexpr uint32_t server_init_read = 2;

constexpr uint32_t abort = 4;

constexpr uint32_t client_block_write = 6;
constexpr uint32_t server_block_write = 5;

constexpr uint32_t client_block_read = 5;
constexpr uint32_t server_block_read = 6;
} // namespace sdo_cs_codes

inline uint32_t get_cs_code(emb::can::frame_t const& frame) {
  return (frame.payload[0] >> 5) & 0x07;
}

// On-wire 4-byte data field of an expedited SDO frame.
using expedited_sdo_data = std::array<uint8_t, 4>;

// User-facing typed OD value. Covers every scalar type representable in a
// 4-byte expedited SDO payload.
using od_value = std::
    variant<bool, int8_t, int16_t, int32_t, uint8_t, uint16_t, uint32_t, float>;

struct expedited_sdo {
  uint32_t data_size_indicated : 1;
  uint32_t expedited_transfer : 1;
  uint32_t data_empty_bytes : 2;
  uint32_t _reserved : 1;
  uint32_t cs : 3;
  uint32_t index : 16;
  uint32_t subindex : 8;
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

enum class sdo_abort_code : uint32_t {
  invalid_cs = 0x05040001,
  unsupported_access = 0x06010000,
  read_access_wo = 0x06010001,
  write_access_ro = 0x06010002,
  object_not_found = 0x06020000,
  hardware_error = 0x06060000,
  value_range_exceeded = 0x06090030,
  value_too_high = 0x06090031,
  value_too_low = 0x06090032,
  general_error = 0x08000000,
  data_store_error = 0x08000020,
  local_control_error = 0x08000021,
  state_error = 0x08000022
};

struct abort_sdo {
  uint32_t _reserved : 5;
  uint32_t cs : 3;
  uint32_t index : 16;
  uint32_t subindex : 8;
  uint32_t error_code;
  abort_sdo() = default;

  abort_sdo(uint16_t index_, uint8_t subindex_, sdo_abort_code error_code_)
      : _reserved(0),
        cs(sdo_cs_codes::abort),
        index(index_),
        subindex(subindex_),
        error_code(std::to_underlying(error_code_)) {}

  bool valid() const {
    return cs == sdo_cs_codes::abort;
  }
};

enum class od_value_type : uint8_t {
  boolean,
  int8,
  int16,
  int32,
  uint8,
  uint16,
  uint32,
  float32,
  exec,
  string
};

enum class od_access : uint8_t { rw, ro, wo, const_ };

// ---- od accessors ----

inline std::expected<od_value, sdo_abort_code> od_no_read() {
  return std::unexpected(sdo_abort_code::unsupported_access);
}

inline std::expected<void, sdo_abort_code> od_no_write(od_value /*val*/) {
  return std::unexpected(sdo_abort_code::unsupported_access);
}

// Helpers for plain-variable entries. NTTP requires static storage.
template<auto Var>
std::expected<od_value, sdo_abort_code> od_read_var() {
  return od_value{*Var};
}

template<auto Var>
std::expected<void, sdo_abort_code> od_write_var(od_value val) {
  using T = std::remove_pointer_t<decltype(Var)>;
  if (auto* v = std::get_if<T>(&val)) {
    *Var = *v;
    return {};
  }
  return std::unexpected(sdo_abort_code::general_error);
}

// Deserialize raw 4-byte SDO data into a typed od_value per the OD entry's
// declared data_type. `exec` entries are forwarded as uint32 — they don't
// carry a semantic value; the user write_func interprets the bytes as a
// magic command (e.g. "save"/"load"). `string` returns uint32{0} — not
// supported by the current SDO path (requires block transfer).
inline od_value make_od_value(expedited_sdo_data raw, od_value_type type) {
  switch (type) {
  case od_value_type::boolean: return raw[0] != 0;
  case od_value_type::int8: return static_cast<int8_t>(raw[0]);
  case od_value_type::int16: {
    int16_t v;
    std::memcpy(&v, raw.data(), sizeof(v));
    return v;
  }
  case od_value_type::int32: {
    int32_t v;
    std::memcpy(&v, raw.data(), sizeof(v));
    return v;
  }
  case od_value_type::uint8: return raw[0];
  case od_value_type::uint16: {
    uint16_t v;
    std::memcpy(&v, raw.data(), sizeof(v));
    return v;
  }
  case od_value_type::uint32:
  case od_value_type::exec: {
    uint32_t v;
    std::memcpy(&v, raw.data(), sizeof(v));
    return v;
  }
  case od_value_type::float32: {
    float v;
    std::memcpy(&v, raw.data(), sizeof(v));
    return v;
  }
  default: return uint32_t{0};
  }
}

// Serialize a typed od_value back into raw 4 bytes.
inline expedited_sdo_data to_raw(od_value v) {
  expedited_sdo_data raw{};
  v.visit([&](auto const& x) { std::memcpy(raw.data(), &x, sizeof(x)); });
  return raw;
}

constexpr std::array<size_t, 10> od_data_type_sizes = {
    sizeof(bool),
    sizeof(int8_t),
    sizeof(int16_t),
    sizeof(int32_t),
    sizeof(uint8_t),
    sizeof(uint16_t),
    sizeof(uint32_t),
    sizeof(float),
    4,
    4
};

struct od_key {
  uint16_t index;
  uint8_t subindex;
};

struct od_object {
  char const* category;
  char const* subcategory;
  char const* name;
  char const* unit;
  od_access access;
  od_value_type data_type;
  std::optional<od_value> default_value;
  std::expected<od_value, sdo_abort_code> (*read)();
  std::expected<void, sdo_abort_code> (*write)(od_value val);

  bool has_read_permission() const {
    return access != od_access::wo;
  }

  bool has_write_permission() const {
    return (access == od_access::rw) || (access == od_access::wo);
  }
};

struct od_entry {
  od_key key;
  od_object object;
};

inline bool operator<(od_entry const& lhs, od_entry const& rhs) {
  return (lhs.key.index < rhs.key.index)
      || ((lhs.key.index == rhs.key.index)
          && (lhs.key.subindex < rhs.key.subindex));
}

inline bool operator<(od_entry const& lhs, od_key const& rhs) {
  return (lhs.key.index < rhs.index)
      || ((lhs.key.index == rhs.index) && (lhs.key.subindex < rhs.subindex));
}

inline bool operator<(od_key const& lhs, od_entry const& rhs) {
  return (lhs.index < rhs.key.index)
      || ((lhs.index == rhs.key.index) && (lhs.subindex < rhs.key.subindex));
}

inline bool operator==(od_key const& lhs, od_entry const& rhs) {
  return (lhs.index == rhs.key.index) && (lhs.subindex == rhs.key.subindex);
}

inline bool operator==(od_key const& lhs, od_key const& rhs) {
  return (lhs.index == rhs.index) && (lhs.subindex == rhs.subindex);
}

} // namespace canopen
} // namespace emb
