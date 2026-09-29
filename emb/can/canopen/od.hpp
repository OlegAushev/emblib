#pragma once

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <string_view>
#include <utility>
#include <variant>

#include "sdo.hpp"

#include <emb/meta/alternative_of.hpp>

namespace emb {
namespace can {
namespace canopen {

// User-facing typed OD value. Covers every scalar type representable in a
// 4-byte expedited SDO payload.
using od_value = std::variant<bool,
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

// The codes of the scalar types are od_value's alternative indices, so
// od_type_of below is an index and the two cannot silently drift apart.
static_assert(std::variant_size_v<od_value> == 8);
static_assert(std::same_as<std::variant_alternative_t<0, od_value>, bool>);
static_assert(
    std::same_as<std::variant_alternative_t<1, od_value>, std::int8_t>);
static_assert(
    std::same_as<std::variant_alternative_t<2, od_value>, std::int16_t>);
static_assert(
    std::same_as<std::variant_alternative_t<3, od_value>, std::int32_t>);
static_assert(
    std::same_as<std::variant_alternative_t<4, od_value>, std::uint8_t>);
static_assert(
    std::same_as<std::variant_alternative_t<5, od_value>, std::uint16_t>);
static_assert(
    std::same_as<std::variant_alternative_t<6, od_value>, std::uint32_t>);
static_assert(std::same_as<std::variant_alternative_t<7, od_value>, float>);
static_assert(std::to_underlying(od_value_type::float32) == 7);

// The type code of the scalar `T`.
template<od_scalar T>
inline constexpr od_value_type od_type_of =
    static_cast<od_value_type>(od_value(std::in_place_type<T>).index());

// Returns the index of the od_value alternative an object of type `type`
// travels as: its own for a scalar type, uint32's for exec and string.
constexpr std::size_t od_alternative_of(od_value_type type)
{
  switch (type) {
  case od_value_type::exec:
  case od_value_type::string: return std::to_underlying(od_value_type::uint32);
  default: return std::to_underlying(type);
  }
}

enum class od_access : std::uint8_t { rw, ro, wo, const_ };

constexpr bool od_readable(od_access access)
{
  return access != od_access::wo;
}

constexpr bool od_writable(od_access access)
{
  return (access == od_access::rw) || (access == od_access::wo);
}

// Accessor result types: what an object's reader and writer return.
using od_read_result = std::expected<od_value, sdo_abort_code>;
using od_write_result = std::expected<void, sdo_abort_code>;

// Deserialize raw 4-byte SDO data into a typed od_value per the OD entry's
// declared data_type. `exec` entries are forwarded as uint32 — they don't
// carry a semantic value; the user write_func interprets the bytes as a
// magic command (e.g. "save"/"load"). `string` returns uint32{0} — not
// supported by the current SDO path (requires block transfer).
inline od_value make_od_value(expedited_sdo_data raw, od_value_type type)
{
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
inline expedited_sdo_data to_raw(od_value v)
{
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
    4};

// The struct `od_key` is the address of an object: its index and subindex.
// Keys are ordered by index, then by subindex.
struct od_key {
  std::uint16_t index;
  std::uint8_t subindex;

  friend constexpr bool operator==(od_key, od_key) = default;
  friend constexpr auto operator<=>(od_key, od_key) = default;
};

// The key the server serves itself: a write to 1011h:04 restores the default
// of one object. Its data carries the key of that object, the index in bytes
// 0-1 (little-endian) and the subindex in byte 2; byte 3 is not read.
inline constexpr od_key od_restore_default_key{0x1011, 0x04};

// An object's handlers. `arg` is the argument its row binds, e.g. the index
// of a settings parameter; a reader of a string object gets the number of
// the 4-byte word to return instead.
template<typename Ctx>
using od_read_fn = od_read_result (*)(Ctx&, std::uint16_t arg);

template<typename Ctx>
using od_write_fn = od_write_result (*)(Ctx&, std::uint16_t arg, od_value);

template<typename Ctx>
using od_restore_fn = od_write_result (*)(Ctx&, std::uint16_t arg);

// The struct `od_catalog` names the items of an index space outside the
// dictionary, e.g. the parameters of a settings schema, that rows serve
// through their `arg`. `names` and `exposed` have one element per item;
// `make_dictionary` checks that each exposed item is served by exactly one
// row and each hidden item by none. `what` names an item in diagnostics.
struct od_catalog {
  std::string_view what;
  std::span<std::string_view const> names;
  std::span<bool const> exposed;
};

// The class template `od_binding` describes what serves an object: its access
// and type, its handlers, the argument passed to them, and, for an object
// whose default 1011h:04 may restore, the handler that restores it. The
// builders in od_handlers.hpp and od_settings.hpp make one from typed
// handlers; one made by hand must have a reader exactly if the access is
// readable and a writer exactly if it is writable, and its reader must return
// the alternative its type travels as. `catalog` is the index space `arg`
// selects from, if any. `diagnosed` is set by a builder that has already
// reported an error, so that `make_dictionary` does not report a second one.
template<typename Ctx>
struct od_binding {
  od_access access;
  od_value_type type;
  od_read_fn<Ctx> read = nullptr;
  od_write_fn<Ctx> write = nullptr;
  std::uint16_t arg = 0;
  od_restore_fn<Ctx> restore = nullptr;
  od_catalog const* catalog = nullptr;
  bool diagnosed = false;
};

// The class template `od_row` is one object as an application writes it: its
// key, the names a host finds it by, its unit, its type and its binding. The
// type is stated in the row, although the binding determines it, so that a
// text parser can generate a host's table from the application's source;
// `make_dictionary` checks that the two agree. Rows exist only at compile
// time: `make_dictionary` keeps the key and the binding and drops the rest.
template<typename Ctx>
struct od_row {
  od_key key;
  std::string_view category;
  std::string_view subcategory;
  std::string_view name;
  std::string_view unit;
  od_value_type type;
  od_binding<Ctx> binding;
};

// The class template `od_entry` is what the server keeps of a row: the key,
// the type, the access, whether 1011h:04 may restore the object, the argument
// and the two handlers. On a 32-bit target it takes 16 bytes.
template<typename Ctx>
struct od_entry {
  std::uint16_t index;
  std::uint8_t subindex;
  od_value_type type;
  od_access access;
  bool restorable;
  std::uint16_t arg;
  od_read_fn<Ctx> read;
  od_write_fn<Ctx> write;

  constexpr od_key key() const
  {
    return {index, subindex};
  }
};

} // namespace canopen
} // namespace can
} // namespace emb
