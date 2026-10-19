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

enum class od_data_type : std::uint8_t {
  boolean,
  int8,
  int16,
  int32,
  uint8,
  uint16,
  uint32,
  float32,
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
static_assert(std::to_underlying(od_data_type::float32) == 7);

// The type code of the scalar `T`.
template<od_scalar T>
inline constexpr od_data_type od_type_of =
    static_cast<od_data_type>(od_value(std::in_place_type<T>).index());

// Returns the index of the od_value alternative an object of type `type`
// travels as: its own for a scalar type, uint32's for a string.
constexpr std::size_t od_alternative_of(od_data_type type)
{
  switch (type) {
  case od_data_type::string: return std::to_underlying(od_data_type::uint32);
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

// Returns the value that `bytes`, the data of an expedited SDO, carries for
// an object of type `type`: the scalar of that type in its leading bytes,
// whatever the bytes after it hold. A `string` object is never written, and
// reads as `uint32{0}`.
inline od_value from_sdo_data(expedited_sdo_data bytes, od_data_type type)
{
  switch (type) {
  case od_data_type::boolean: return bytes[0] != 0;
  case od_data_type::int8: return static_cast<std::int8_t>(bytes[0]);
  case od_data_type::int16: {
    std::int16_t v;
    std::memcpy(&v, bytes.data(), sizeof(v));
    return v;
  }
  case od_data_type::int32: {
    std::int32_t v;
    std::memcpy(&v, bytes.data(), sizeof(v));
    return v;
  }
  case od_data_type::uint8: return bytes[0];
  case od_data_type::uint16: {
    std::uint16_t v;
    std::memcpy(&v, bytes.data(), sizeof(v));
    return v;
  }
  case od_data_type::uint32: {
    std::uint32_t v;
    std::memcpy(&v, bytes.data(), sizeof(v));
    return v;
  }
  case od_data_type::float32: {
    float v;
    std::memcpy(&v, bytes.data(), sizeof(v));
    return v;
  }
  default: return std::uint32_t{0};
  }
}

// Returns the data of an expedited SDO that carries `v`: the bytes of its
// scalar, followed by zeros.
inline expedited_sdo_data to_sdo_data(od_value v)
{
  expedited_sdo_data bytes{};
  v.visit([&](auto const& x) { std::memcpy(bytes.data(), &x, sizeof(x)); });
  return bytes;
}

// Returns the size, in bytes, of an object of type `type` in the data of an
// expedited SDO: that of its scalar, and 4 for a `string`, which travels as
// `uint32` words.
constexpr std::size_t od_size_of(od_data_type type)
{
  switch (type) {
  case od_data_type::boolean:
  case od_data_type::int8:
  case od_data_type::uint8: return 1;
  case od_data_type::int16:
  case od_data_type::uint16: return 2;
  case od_data_type::int32:
  case od_data_type::uint32:
  case od_data_type::float32:
  case od_data_type::string: return 4;
  }
  return 4;
}

// The struct `od_key` is the address of an object: its index and subindex.
// Keys are ordered by index, then by subindex.
struct od_key {
  std::uint16_t index;
  std::uint8_t subindex;

  friend constexpr bool operator==(od_key, od_key) = default;
  friend constexpr auto operator<=>(od_key, od_key) = default;
};

// An object's handlers. `arg` is the argument its row binds, e.g. the index
// of a settings parameter; a reader of a string object gets the number of
// the 4-byte word to return instead.
template<typename Ctx>
using od_read_fn = od_read_result (*)(Ctx&, std::uint16_t arg);

template<typename Ctx>
using od_write_fn = od_write_result (*)(Ctx&, std::uint16_t arg, od_value);

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

// The class template `od_field` describes a member of a record: its access
// and type, and its handlers, which get the argument of the record. A layout,
// an array of fields indexed by subindex from 00h, describes every member of a
// record; any number of records may share one, each with an argument of its
// own. A field obeys the rules of a binding made by hand.
template<typename Ctx>
struct od_field {
  od_access access;
  od_data_type type;
  od_read_fn<Ctx> read = nullptr;
  od_write_fn<Ctx> write = nullptr;
};

// The class template `od_binding` describes what serves an object: its access
// and type, its handlers and the argument passed to them. The builders in
// od_handlers.hpp and od_settings.hpp make one from typed handlers; one made
// by hand must have a reader exactly if the access is readable and a writer
// exactly if it is writable, and its reader must return the alternative its
// type travels as. `catalog` is the index space `arg` selects from, if any.
// `diagnosed` is set by a builder that has already reported an error, so that
// `make_dictionary` does not report a second one.
//
// A binding with a `layout` serves a record instead: every subindex of the
// index of its row, which names it with subindex 00h, each through the field
// of the layout at that subindex. It has no handlers of its own, and its
// `access` is not used; `type` is what the row states, and `arg` the argument
// its fields pass on.
template<typename Ctx>
struct od_binding {
  od_access access;
  od_data_type type;
  od_read_fn<Ctx> read = nullptr;
  od_write_fn<Ctx> write = nullptr;
  std::uint16_t arg = 0;
  od_catalog const* catalog = nullptr;
  std::span<od_field<Ctx> const> layout{};
  bool diagnosed = false;
};

// The class template `od_row` is one object, or one record, as an application
// writes it: its key, the names a host finds it by, its unit, its type and its
// binding. The type is stated in the row, although the binding determines it,
// so that a text parser can generate a host's table from the application's
// source; `make_dictionary` checks that the two agree. Rows exist only at
// compile time: `make_dictionary` keeps the key and the binding and drops the
// rest.
template<typename Ctx>
struct od_row {
  od_key key;
  std::string_view category;
  std::string_view subcategory;
  std::string_view name;
  std::string_view unit;
  od_data_type type;
  od_binding<Ctx> binding;
};

// The class template `od_entry` is an object as the server serves it: the key,
// the type, the access, the argument and the two handlers, kept for a row or
// made of a field of a record. On a 32-bit target it takes 16 bytes.
template<typename Ctx>
struct od_entry {
  std::uint16_t index;
  std::uint8_t subindex;
  od_data_type type;
  od_access access;
  std::uint16_t arg;
  od_read_fn<Ctx> read;
  od_write_fn<Ctx> write;

  constexpr od_key key() const
  {
    return {index, subindex};
  }
};

// The class template `od_record` is what the server keeps of a record: its
// index, its argument and its layout. On a 32-bit target it takes 12 bytes.
template<typename Ctx>
struct od_record {
  std::uint16_t index;
  std::uint16_t arg;
  std::span<od_field<Ctx> const> layout;
};

} // namespace canopen
} // namespace can
} // namespace emb
