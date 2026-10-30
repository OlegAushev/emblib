#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "../od.hpp"
#include "../od_process_data.hpp"
#include "../types.hpp"
#include "od_check.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

// The sinks below take the text of a description of a dictionary through
// `put`, a character or a view at a time. None builds a `std::string`: GCC
// takes gigabytes to grow one to the size of a description in constant
// evaluation.

// The struct `od_counting_sink` counts the characters it takes.
struct od_counting_sink {
  std::size_t size = 0;

  constexpr void put(char)
  {
    ++size;
  }

  constexpr void put(std::string_view text)
  {
    size += text.size();
  }
};

// The struct `od_hashing_sink` hashes the characters it takes: `hash` is the
// 32-bit FNV-1a hash of all of them.
struct od_hashing_sink {
  std::uint32_t hash = 0x811C9DC5u;

  constexpr void put(char c)
  {
    hash = (hash ^ static_cast<std::uint8_t>(c)) * 0x01000193u;
  }

  constexpr void put(std::string_view text)
  {
    for (char c : text) {
      hash = (hash ^ static_cast<std::uint8_t>(c)) * 0x01000193u;
    }
  }
};

// The class template `od_writing_sink` writes the characters it takes into
// `text`, from `size` on, and hashes them as `od_hashing_sink` does.
template<std::size_t N>
struct od_writing_sink {
  std::array<char, N>& text;
  std::size_t size = 0;
  od_hashing_sink hashing{};

  constexpr void put(char c)
  {
    text[size++] = c;
    hashing.put(c);
  }

  constexpr void put(std::string_view chars)
  {
    for (char c : chars) {
      text[size++] = c;
      hashing.put(c);
    }
  }
};

// The version of the format of a description, which grows only when a host
// that reads the present one could no longer read a new one.
inline constexpr unsigned od_description_version = 1;

// Writes `value` as a string of JSON: "0x" and `digits` uppercase hexadecimal
// digits.
template<typename Sink>
constexpr void put_hex(Sink& sink, unsigned value, int digits)
{
  constexpr std::string_view hex = "0123456789ABCDEF";
  sink.put("\"0x");
  for (int shift = 4 * (digits - 1); shift >= 0; shift -= 4) {
    sink.put(hex[(value >> shift) & 0xFu]);
  }
  sink.put('"');
}

// Writes `value` as a number of JSON: decimal digits, no leading zero.
template<typename Sink>
constexpr void put_decimal(Sink& sink, std::uint64_t value)
{
  char digits[20]{};
  auto count = 0uz;
  do {
    digits[count++] = static_cast<char>('0' + value % 10);
    value /= 10;
  } while (value != 0);
  while (count != 0) {
    sink.put(digits[--count]);
  }
}

// Writes `value` as a number of JSON: decimal digits, no leading zero, after
// a minus sign if it is negative.
template<typename Sink>
constexpr void put_signed(Sink& sink, std::int64_t value)
{
  if (value < 0) sink.put('-');
  put_decimal(sink,
              value < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(value)
                        : static_cast<std::uint64_t>(value));
}

// Writes the separator and the name of a member of an object of JSON, after
// the first one: `,"key":`.
template<typename Sink>
constexpr void put_key(Sink& sink, std::string_view key)
{
  sink.put(",\"");
  sink.put(key);
  sink.put("\":");
}

// Writes a member whose value is a string; `value` needs no escapes, as the
// rules of `od_check` see to it.
template<typename Sink>
constexpr void put_string(Sink& sink,
                          std::string_view key,
                          std::string_view value)
{
  put_key(sink, key);
  sink.put('"');
  sink.put(value);
  sink.put('"');
}

// Writes a flag, which a description states only when it is set.
template<typename Sink>
constexpr void put_flag(Sink& sink, std::string_view key, bool value)
{
  if (!value) return;
  put_key(sink, key);
  sink.put("true");
}

// Writes a text for people: the text of each language under `key`, an
// underscore and the code of the language, e.g. `label_en`. The texts need no
// escapes, as the rules of `od_check` see to it.
template<typename Sink>
constexpr void put_text(Sink& sink,
                        std::string_view key,
                        i18n::text const& text)
{
  for (auto const& language : i18n::languages) {
    sink.put(",\"");
    sink.put(key);
    sink.put('_');
    sink.put(language.code);
    sink.put("\":\"");
    sink.put(text.*language.member);
    sink.put('"');
  }
}

// Writes a label, under `label_` and the code of each language, e.g.
// `label_en`.
template<typename Sink>
constexpr void put_label(Sink& sink, i18n::text const& label)
{
  put_text(sink, "label", label);
}

// Writes the symbol of a unit, under `unit_` and the code of each language,
// e.g. `unit_en`, unless there is no unit, `i18n::text{}`.
template<typename Sink>
constexpr void put_unit(Sink& sink, i18n::text const& unit)
{
  if (!unit.empty()) put_text(sink, "unit", unit);
}

// Writes the start of a line of a description: `{"type":"kind"`.
template<typename Sink>
constexpr void put_start(Sink& sink, std::string_view kind)
{
  sink.put("{\"type\":\"");
  sink.put(kind);
  sink.put('"');
}

// Writes the end of a line of a description.
template<typename Sink>
constexpr void put_end(Sink& sink)
{
  sink.put("}\n");
}

// Writes the header of a description, its first line: the format, its
// version, the name of the device and the fingerprint, which takes as many
// characters whatever its value.
template<typename Sink>
constexpr void put_header(Sink& sink,
                          std::string_view device,
                          std::uint32_t fingerprint)
{
  put_start(sink, "header");
  put_string(sink, "format", "emb-canopen-od");
  put_key(sink, "version");
  put_decimal(sink, od_description_version);
  put_string(sink, "device", device);
  put_key(sink, "fingerprint");
  put_hex(sink, fingerprint, 8);
  put_end(sink);
}

// Whether the tables of values `a` and `b` name the same values alike: as
// many values, in the same order, each with the same name and labels.
constexpr bool same_values(std::span<od_value_info const> a,
                           std::span<od_value_info const> b)
{
  if (a.size() != b.size()) return false;
  if (a.data() == b.data()) return true;
  for (auto i = 0uz; i < a.size(); ++i) {
    if (a[i].value != b[i].value
        || a[i].name != b[i].name
        || a[i].label != b[i].label) {
      return false;
    }
  }
  return true;
}

// The tables of values of a description, in the order of their numbers, each
// table once, as `same_values` compares them.
using od_value_tables = std::vector<std::span<od_value_info const>>;

// Returns the number of `values` among `tables`, or `tables.size()` if it is
// not there.
constexpr std::size_t table_number(od_value_tables const& tables,
                                   std::span<od_value_info const> values)
{
  auto k = 0uz;
  while (k < tables.size() && !same_values(tables[k], values)) {
    ++k;
  }
  return k;
}

// Adds `values` to `tables` unless it is there already or empty.
constexpr void add_table(od_value_tables& tables,
                         std::span<od_value_info const> values)
{
  if (!values.empty() && table_number(tables, values) == tables.size()) {
    tables.push_back(values);
  }
}

// Writes the number of the table of values `values` among `tables`, unless
// it is empty: `,"value_table":0`.
template<typename Sink>
constexpr void put_table(Sink& sink,
                         od_value_tables const& tables,
                         std::span<od_value_info const> values)
{
  if (values.empty()) return;
  put_key(sink, "value_table");
  put_decimal(sink, table_number(tables, values));
}

// Whether the records `a` and `b` show a host the same layout: as many
// members, and each with the same name, label, access, flags, signature and
// table of values, and the same type unless it inherits the record's. Records
// of parameters of different types thus share a layout, as do layouts that
// differ only in their handlers.
template<typename Ctx>
constexpr bool same_layout(od_binding<Ctx> const& a, od_binding<Ctx> const& b)
{
  if (a.layout.size() != b.layout.size()) return false;
  // The info of the members of records of one kind is one array, compared
  // once.
  bool const shared = a.field_info.data() == b.field_info.data();
  for (auto i = 0uz; i < a.layout.size(); ++i) {
    auto const& la = a.field_info[i];
    auto const& lb = b.field_info[i];
    if (!shared
        && (la.name != lb.name
            || la.label != lb.label
            || la.inherits != lb.inherits
            || la.command != lb.command
            || la.signature != lb.signature
            || !same_values(la.value_table, lb.value_table))) {
      return false;
    }
    if (a.layout[i].access != b.layout[i].access) return false;
    if (!la.inherits && a.layout[i].type != b.layout[i].type) return false;
  }
  return true;
}

// Writes the lines of the values of `tables`, by table, and in a table by
// value.
template<typename Sink>
constexpr void put_values(Sink& sink, od_value_tables const& tables)
{
  for (auto k = 0uz; k < tables.size(); ++k) {
    for (auto const& info : tables[k]) {
      put_start(sink, "value");
      put_key(sink, "value_table");
      put_decimal(sink, k);
      put_key(sink, "value");
      put_signed(sink, info.value);
      put_string(sink, "name", info.name);
      put_label(sink, info.label);
      put_end(sink);
    }
  }
}

// Writes the line of the member `sub` of layout number `layout`, whose fields
// and info are those of the record `binding`, its table of values numbered
// among `tables`.
template<typename Sink, typename Ctx>
constexpr void put_field(Sink& sink,
                         std::size_t layout,
                         std::size_t sub,
                         od_binding<Ctx> const& binding,
                         od_value_tables const& tables)
{
  auto const& field = binding.layout[sub];
  auto const& member = binding.field_info[sub];
  put_start(sink, "field");
  put_key(sink, "layout");
  put_decimal(sink, layout);
  put_key(sink, "sub");
  put_hex(sink, static_cast<unsigned>(sub), 2);
  put_string(sink, "name", member.name);
  put_label(sink, member.label);
  if (!member.inherits) put_string(sink, "datatype", type_name(field.type));
  put_string(sink, "access", access_name(field.access));
  put_table(sink, tables, member.value_table);
  put_flag(sink, "command", member.command);
  if (member.signature) {
    put_key(sink, "signature");
    put_hex(sink, *member.signature, 8);
  }
  put_flag(sink, "inherits", member.inherits);
  put_end(sink);
}

// Writes the line of `row`, of layout number `layout` if it binds a record,
// in the area `area`, its table of values numbered among `tables`, and the
// group and the name of its path as two keys, which a host reads without
// splitting a string.
template<typename Sink, typename Ctx>
constexpr void put_row(Sink& sink,
                       od_row<Ctx> const& row,
                       od_area const* area,
                       std::size_t layout,
                       od_value_tables const& tables)
{
  auto const& binding = row.binding;
  auto const& info = info_of(row);
  bool const record = !binding.layout.empty();
  bool const array = binding.count != nullptr;

  put_start(sink, record ? "record" : array ? "array" : "object");
  put_key(sink, "index");
  put_hex(sink, row.key.index, 4);
  if (!record && !array) {
    put_key(sink, "sub");
    put_hex(sink, row.key.subindex, 2);
  }
  put_string(sink, "group", group_of(info.path));
  put_string(sink, "name", name_of(info.path));
  put_label(sink, info.label);
  put_string(sink,
             "class",
             area != nullptr ? class_name(area->class_)
                             : std::string_view{"?"});
  if (!record || inherits_value(binding)) {
    put_string(sink, "datatype", type_name(binding.type));
  }
  if (!record) put_string(sink, "access", access_name(binding.access));
  put_unit(sink, unit_of(row));
  put_table(sink, tables, binding.value_table);
  if (record) {
    put_key(sink, "layout");
    put_decimal(sink, layout);
  }
  put_flag(sink, "text", binding.text);
  put_flag(sink, "command", binding.command);
  if (binding.signature) {
    put_key(sink, "signature");
    put_hex(sink, *binding.signature, 8);
  }
  put_end(sink);
}

// Writes the line of the PDO `mapped`: its name, its label, its direction,
// its number, what it carries, `signals` or, of an MPDO in source address
// mode, `sam`, its COB-ID, custom in `cob_id`, or the base of its predefined
// form in `cob_base`, which a host adds the node-ID to, and the period of a
// TPDO or the timeout of an RPDO, in milliseconds. The line does not list the
// objects of an MPDO: each frame names its own.
template<typename Sink>
constexpr void put_pdo(Sink& sink, od_mapped_pdo const& mapped)
{
  auto const& pdo = mapped.pdo;
  bool const tx = pdo.direction == pdo_direction::tx;
  put_start(sink, "pdo");
  put_string(sink, "name", pdo.name);
  put_label(sink, pdo.label);
  put_string(sink, "direction", direction_name(pdo.direction));
  put_key(sink, "number");
  put_decimal(sink, pdo.number);
  put_string(sink, "mapping", mapping_name(mapped.mapping()));
  if (pdo.cob_id.is_custom) {
    put_key(sink, "cob_id");
    put_hex(sink, pdo.cob_id.value, 3);
  }
  else {
    put_key(sink, "cob_base");
    put_hex(sink, cob_base_of(pdo.direction, pdo.number), 3);
  }
  put_key(sink, tx ? "period_ms" : "timeout_ms");
  put_decimal(
      sink,
      static_cast<std::uint64_t>((tx ? pdo.period : pdo.timeout).count()));
  put_end(sink);
}

// Writes the line of `signal`, a signal of `pdo`, its table of values
// numbered among `tables`: where its bits lie, its role, the name and the
// label of a signal of the role `value`, how its bits read, its step unless
// it is 1/1, its unit and its table of values, if any.
template<typename Sink>
constexpr void put_signal(Sink& sink,
                          od_pdo const& pdo,
                          od_signal const& signal,
                          od_value_tables const& tables)
{
  put_start(sink, "signal");
  put_string(sink, "pdo", pdo.name);
  put_key(sink, "start");
  put_decimal(sink, signal.bits.start);
  put_key(sink, "width");
  put_decimal(sink, signal.bits.width);
  put_string(sink, "role", role_name(signal.role));
  if (signal.role == pdo_role::value) {
    put_string(sink, "name", signal.name);
    put_label(sink, signal.label);
  }
  put_string(sink, "encoding", encoding_name(signal.encoding));
  if (signal.scale_num != 1 || signal.scale_den != 1) {
    put_key(sink, "scale");
    sink.put('"');
    put_signed(sink, signal.scale_num);
    sink.put('/');
    put_signed(sink, signal.scale_den);
    sink.put('"');
  }
  put_unit(sink, signal.unit);
  put_table(sink, tables, signal.value_table);
  put_end(sink);
}

// Returns the positions of the PDOs `pdos` by direction, the TPDOs first, and
// by number, PDOs that the two do not tell apart in the order of `pdos`.
constexpr std::vector<std::size_t>
sorted_pdos(std::span<od_mapped_pdo const> pdos)
{
  std::vector<std::size_t> order(pdos.size());
  for (auto i = 0uz; i < order.size(); ++i) {
    order[i] = i;
  }
  auto const key = [&](std::size_t i) {
    return std::pair{pdos[i].pdo.direction, pdos[i].pdo.number};
  };
  std::ranges::sort(order, [&](std::size_t a, std::size_t b) {
    return key(a) < key(b) || (key(a) == key(b) && a < b);
  });
  return order;
}

// Returns the positions of the signals `signals` by their first bits, signals
// that start at one bit in the order of `signals`.
constexpr std::vector<std::size_t>
sorted_signals(std::span<od_signal const> signals)
{
  std::vector<std::size_t> order(signals.size());
  for (auto i = 0uz; i < order.size(); ++i) {
    order[i] = i;
  }
  std::ranges::sort(order, [&](std::size_t a, std::size_t b) {
    auto const x = signals[a].bits.start;
    auto const y = signals[b].bits.start;
    return x < y || (x == y && a < b);
  });
  return order;
}

// Writes the body of the description of the dictionary made of `rows`, whose
// indices lie in `areas` and whose paths start with the groups of `groups`,
// with the PDOs of `process_data` beside it: a line for each group, in the
// order of its first row by key; a line for each value of a table, by table
// and in a table by value, the tables numbered in the order in which the
// members of the layouts, then the rows and last the signals first name them,
// and compared by what a host sees (`same_values`); a line for each member of
// each layout, the layouts numbered in the order of their first records by
// key and compared by what a host sees (`same_layout`); a line for each row,
// in the order of keys; and a line for each PDO, by direction, the TPDOs
// first, and by number, each followed by a line for each of its signals, by
// first bit, of which an MPDO in source address mode has none. Neither the
// order of the rows, of the groups or of the PDOs in their arrays nor that of
// the rows of a PDO changes the text, and without PDOs it is that of the
// dictionary alone.
template<typename Sink,
         typename Ctx,
         std::size_t N,
         std::size_t A,
         std::size_t G>
constexpr void od_describe(Sink& sink,
                           od_row<Ctx> const (&rows)[N],
                           od_area const (&areas)[A],
                           od_group const (&groups)[G],
                           od_process_data const& process_data)
{
  auto const order = sorted_positions(rows, [](auto const& a, auto const& b) {
    return a.key < b.key;
  });

  std::array<bool, G> seen{};
  for (auto const i : order) {
    auto const group = group_position(group_of(info_of(rows[i]).path), groups);
    if (group == G || seen[group]) continue;
    seen[group] = true;
    put_start(sink, "group");
    put_string(sink, "name", groups[group].name);
    put_label(sink, groups[group].label);
    put_end(sink);
  }

  // The layout number of each row of a record, and the first record of each
  // layout.
  std::array<std::size_t, N> layout_of{};
  std::array<std::size_t, N> first_record{};
  auto layouts = 0uz;
  for (auto const i : order) {
    auto const& binding = rows[i].binding;
    if (binding.layout.empty()) continue;
    auto k = 0uz;
    while (k < layouts
           && !same_layout(rows[first_record[k]].binding, binding)) {
      ++k;
    }
    if (k == layouts) first_record[layouts++] = i;
    layout_of[i] = k;
  }

  // The PDOs in the order of their lines, and the signals of each.
  auto const& pdos = process_data.pdos;
  auto const pdo_order = sorted_pdos(pdos);
  std::vector<std::vector<std::size_t>> signal_order;
  for (auto const p : pdo_order) {
    signal_order.push_back(sorted_signals(pdos[p].signals));
  }

  // The tables of values: those of the members of the layouts, by layout and
  // by subindex, then those of the rows, by key, and last those of the
  // signals, in the order of their lines.
  od_value_tables tables;
  for (auto k = 0uz; k < layouts; ++k) {
    for (auto const& member : rows[first_record[k]].binding.field_info) {
      add_table(tables, member.value_table);
    }
  }
  for (auto const i : order) {
    add_table(tables, rows[i].binding.value_table);
  }
  for (auto k = 0uz; k < pdo_order.size(); ++k) {
    for (auto const s : signal_order[k]) {
      add_table(tables, pdos[pdo_order[k]].signals[s].value_table);
    }
  }
  put_values(sink, tables);

  for (auto k = 0uz; k < layouts; ++k) {
    auto const& binding = rows[first_record[k]].binding;
    for (auto sub = 0uz; sub < binding.layout.size(); ++sub) {
      put_field(sink, k, sub, binding, tables);
    }
  }

  for (auto const i : order) {
    put_row(sink,
            rows[i],
            area_of(rows[i].key.index, areas),
            layout_of[i],
            tables);
  }

  for (auto k = 0uz; k < pdo_order.size(); ++k) {
    auto const& mapped = pdos[pdo_order[k]];
    put_pdo(sink, mapped);
    for (auto const s : signal_order[k]) {
      put_signal(sink, mapped.pdo, mapped.signals[s], tables);
    }
  }
}

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
