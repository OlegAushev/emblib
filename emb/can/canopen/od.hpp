#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include "sdo.hpp"

#include <emb/meta/alternative_of.hpp>

namespace emb {
namespace can {
namespace canopen {

// User-facing typed OD value. Covers every scalar type representable in a
// 4-byte expedited SDO payload.
using od_value = std::variant<
    bool,
    std::int8_t,
    std::int16_t,
    std::int32_t,
    std::uint8_t,
    std::uint16_t,
    std::uint32_t,
    float>;

// T is one of od_value's alternatives; derived from od_value itself so the
// two cannot drift apart.
template<typename T>
concept od_scalar = alternative_of<T, od_value>;

enum class od_value_type : std::uint8_t {
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

enum class od_access : std::uint8_t { rw, ro, wo, const_ };

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
  return std::unexpected(sdo_abort_code::data_type_mismatch);
}

// Deserialize raw 4-byte SDO data into a typed od_value per the OD entry's
// declared data_type. `exec` entries are forwarded as uint32 — they don't
// carry a semantic value; the user write_func interprets the bytes as a
// magic command (e.g. "save"/"load"). `string` returns uint32{0} — not
// supported by the current SDO path (requires block transfer).
inline od_value make_od_value(expedited_sdo_data raw, od_value_type type) {
  switch (type) {
  case od_value_type::boolean: return raw[0] != 0;
  case od_value_type::int8: return static_cast<std::int8_t>(raw[0]);
  case od_value_type::int16: {
    std::int16_t v;
    std::memcpy(&v, raw.data(), sizeof(v));
    return v;
  }
  case od_value_type::int32: {
    std::int32_t v;
    std::memcpy(&v, raw.data(), sizeof(v));
    return v;
  }
  case od_value_type::uint8: return raw[0];
  case od_value_type::uint16: {
    std::uint16_t v;
    std::memcpy(&v, raw.data(), sizeof(v));
    return v;
  }
  case od_value_type::uint32:
  case od_value_type::exec: {
    std::uint32_t v;
    std::memcpy(&v, raw.data(), sizeof(v));
    return v;
  }
  case od_value_type::float32: {
    float v;
    std::memcpy(&v, raw.data(), sizeof(v));
    return v;
  }
  default: return std::uint32_t{0};
  }
}

// Serialize a typed od_value back into raw 4 bytes.
inline expedited_sdo_data to_raw(od_value v) {
  expedited_sdo_data raw{};
  v.visit([&](auto const& x) { std::memcpy(raw.data(), &x, sizeof(x)); });
  return raw;
}

constexpr std::array<std::size_t, 10> od_data_type_sizes = {
    sizeof(bool),
    sizeof(std::int8_t),
    sizeof(std::int16_t),
    sizeof(std::int32_t),
    sizeof(std::uint8_t),
    sizeof(std::uint16_t),
    sizeof(std::uint32_t),
    sizeof(float),
    4,
    4
};

struct od_key {
  std::uint16_t index;
  std::uint8_t subindex;
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
} // namespace can
} // namespace emb
