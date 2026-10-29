#pragma once

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>

#include "sdo.hpp"

#include <emb/i18n.hpp>
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
};

// The codes of the data types are od_value's alternative indices, so
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
// whatever the bytes after it hold.
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
  }
  return std::uint32_t{0};
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
// expedited SDO: that of its scalar.
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
  case od_data_type::float32: return 4;
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

// The scoped enumeration `od_class` defines what a host does with an object,
// as the area of its index decides (`od_area`).
enum class od_class : std::uint8_t {
  // An object that describes the device: its name, serial number, versions,
  // build and the fingerprint of its dictionary.
  about,
  // A command, a setpoint, or the state of an operation that a command
  // started, e.g. of applying or saving settings.
  control,
  // A parameter of the device's settings, the state of their storage, or a
  // calibration constant.
  parameters,
  // A status of trouble: whether a condition of the device stands, and at
  // which severity; the device sends it unasked, in an MPDO.
  status,
  // A value that a host polls to show the device at work.
  watch,
};

// The struct `od_area` gives the indices from `first` to `last`, inclusive,
// to objects of the class `class_`. The areas of a dictionary do not overlap,
// and every index of the dictionary lies in one of them.
struct od_area {
  std::uint16_t first;
  std::uint16_t last;
  od_class class_;
};

// The struct `od_group` declares a group of objects: its name, which the path
// of each of its objects starts with, and its title for people. Every group a
// dictionary declares has objects, and none is named as a class.
struct od_group {
  std::string_view name;
  i18n title;
};

// The struct `od_label` labels an object: `path`, "group.name", by which a
// host finds it, its title for people, in each language, and the symbol of
// the unit of its value, empty if it has none. The path and the title are
// required; `unit` comes last, with a default, as most objects have none.
// What serves the object does not depend on its label.
struct od_label {
  std::string_view path;
  i18n title;
  std::string_view unit = {};
};

// The struct `od_value_label` labels, for a host, a value that an object may
// take: the value, its name, a segment of a path unique in its table, by
// which code finds the value, and its title, in each language. A table of
// value labels names the values of an object in increasing order, and the
// object takes those values only: a host shows the title of a value in place
// of the number and offers the values named when it writes one. A builder
// makes the table of an object whose value is an enumeration from the labels
// of the enumeration (`emb::enum_labels_of`). Like the label of a row, a
// table exists at compile time only.
struct od_value_label {
  std::int64_t value;
  std::string_view name;
  i18n title;
};

// An object's handlers. `arg` is the argument its row binds, e.g. the index
// of a settings parameter; the handlers of an array's element get the
// element's position instead, its subindex less one.
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

// The struct `od_field_label` labels a member of a record, a field of its
// layout, for a host: `name`, a segment of a path unique in the layout, the
// title, in each language, whether the member holds the record's value
// (`inherits`), as the requested value of a settings parameter does, and so
// takes the record's type, unit and table of values, whether it is a command
// (`command`), with the word that must be written (`signature`), if any, and
// the table of the values the member takes (`value_labels`), if they are
// named. A label written by hand must tell the truth about its field: nothing
// checks that a command is written to run it, that its signature is the one
// its handler checks, or that its handler reads only the values its table
// names.
struct od_field_label {
  std::string_view name;
  i18n title;
  bool inherits = false;
  bool command = false;
  std::optional<std::uint32_t> signature{};
  std::span<od_value_label const> value_labels{};
};

// The class template `od_binding` describes what serves an object: its access
// and type, its handlers and the argument passed to them. The builders in
// od_handlers.hpp and od_settings.hpp make one from typed handlers; one made
// by hand must have a reader exactly if the access is readable and a writer
// exactly if it is writable, and its reader must return the alternative its
// type travels as. `catalog` is the index space `arg` selects from, if any.
// `label` is the label of the object if the builder knows it, as that of a
// settings parameter knows its name and title from the schema; the row then
// has none of its own. `unit` is the symbol of the unit of the object's value
// as the builder knows it: that of the type its handler reads or writes
// (`emb::unit_symbol_of`), or the unit of a settings parameter; the unit of
// the row's label, if any, takes its place. `value_labels` is the table of the
// values the object takes (`od_value_label`), if they are named, as the
// builder knows it: that of the enumeration its handler reads or writes; of
// an array, the table of its elements, and of a record, of its members that
// inherit its value. `command` marks a command, which
// is written to run it, and `signature`, if it has a value, is the word that
// must be written; `text` marks an array of characters. `diagnosed` is set by
// a builder that has already reported an error, so that `make_dictionary`
// does not report a second one. Of the fields, only the handlers, the
// argument, the access, the type and the layout reach the image.
//
// A binding with a `layout` serves a record instead: every subindex of the
// index of its row, which names it with subindex 00h, each through the field
// of the layout at that subindex. It has no handlers of its own, and its
// `access` is not used; `type` is the type of the record's value, and `arg`
// the argument its fields pass on. `field_labels` labels each field, in the
// order of the layout.
//
// A binding with a `count` serves an array instead: the index of its row,
// which names it with subindex 00h. `count` reads subindex 00h, the number of
// elements as a `uint8`; every other subindex is an element, of the binding's
// `access` and `type`, served by `read` and `write` with the element's
// position, its subindex less one, as their argument. An element past the
// count answers what its handler answers.
template<typename Ctx>
struct od_binding {
  od_access access;
  od_data_type type;
  od_read_fn<Ctx> read = nullptr;
  od_write_fn<Ctx> write = nullptr;
  od_read_fn<Ctx> count = nullptr;
  std::uint16_t arg = 0;
  od_catalog const* catalog = nullptr;
  std::span<od_field<Ctx> const> layout{};
  std::span<od_field_label const> field_labels{};
  od_label label{};
  std::string_view unit{};
  std::span<od_value_label const> value_labels{};
  bool command = false;
  std::optional<std::uint32_t> signature{};
  bool text = false;
  bool diagnosed = false;
};

// The class template `od_row` is one object, one record or one array, as an
// application writes it: its key, its label and its binding. The label is
// empty, `{}`, if the binding gives one. Rows exist only at compile time:
// `make_dictionary` keeps the key and the binding and drops the rest.
template<typename Ctx>
struct od_row {
  od_key key;
  od_label label;
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

// The largest number of elements of an array, at subindices 01h to FEh.
inline constexpr std::size_t od_array_capacity = 0xFE;

// The class template `od_array` is what the server keeps of an array: its
// index, the type and access of its elements, the handler of subindex 00h,
// which reads the number of elements, and those of the elements. On a 32-bit
// target it takes 16 bytes.
template<typename Ctx>
struct od_array {
  std::uint16_t index;
  od_data_type type;
  od_access access;
  od_read_fn<Ctx> count;
  od_read_fn<Ctx> read;
  od_write_fn<Ctx> write;
};

} // namespace canopen
} // namespace can
} // namespace emb
