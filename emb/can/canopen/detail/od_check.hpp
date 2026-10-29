#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../od.hpp"
#include "../od_process_data.hpp"
#include "../types.hpp"
#include "pdo_check.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

constexpr void append_hex(std::string& out, unsigned value, int digits)
{
  constexpr std::string_view hex = "0123456789ABCDEF";
  for (int shift = 4 * (digits - 1); shift >= 0; shift -= 4) {
    out += hex[(value >> shift) & 0xFu];
  }
}

// Appends `value` in decimal digits.
constexpr void append_decimal(std::string& out, std::uint64_t value)
{
  char digits[20]{};
  auto count = 0uz;
  do {
    digits[count++] = static_cast<char>('0' + value % 10);
    value /= 10;
  } while (value != 0);
  while (count != 0) {
    out += digits[--count];
  }
}

// Appends `value` in decimal digits, after a minus sign if it is negative.
constexpr void append_value(std::string& out, std::int64_t value)
{
  if (value < 0) out += '-';
  append_decimal(out,
                 value < 0
                     ? std::uint64_t{0} - static_cast<std::uint64_t>(value)
                     : static_cast<std::uint64_t>(value));
}

// Whether every field of `label` is empty, as the label of a row whose
// binding gives one must be.
constexpr bool is_empty(od_label const& label)
{
  return label.path.empty()
      && label.title.en.empty()
      && label.title.ru.empty()
      && label.unit.empty();
}

// Returns the label of `row`: the one its binding gives, if any, or else its
// own.
template<typename Ctx>
constexpr od_label const& label_of(od_row<Ctx> const& row)
{
  return is_empty(row.binding.label) ? row.label : row.binding.label;
}

// Returns the unit of `row`: the one its label gives, if any, or else the
// one its binding gives.
template<typename Ctx>
constexpr std::string_view unit_of(od_row<Ctx> const& row)
{
  auto const unit = label_of(row).unit;
  return unit.empty() ? row.binding.unit : unit;
}

// Renders a row as "3100h:00 drive.phase_swap", or as "3100h:00" if it has no
// path.
template<typename Ctx>
constexpr void append_row(std::string& out, od_row<Ctx> const& row)
{
  append_hex(out, row.key.index, 4);
  out += "h:";
  append_hex(out, row.key.subindex, 2);
  if (auto const path = label_of(row).path; !path.empty()) {
    out += ' ';
    out += path;
  }
}

// Renders an area as "2000h-20FFh".
constexpr void append_area(std::string& out, od_area const& area)
{
  append_hex(out, area.first, 4);
  out += "h-";
  append_hex(out, area.last, 4);
  out += 'h';
}

template<typename Ctx>
constexpr std::string row_error(od_row<Ctx> const& row, std::string_view what)
{
  std::string out = "od: ";
  append_row(out, row);
  out += ": ";
  out += what;
  return out;
}

template<typename Ctx>
constexpr std::string pair_error(od_row<Ctx> const& first,
                                 od_row<Ctx> const& second,
                                 std::string_view what)
{
  std::string out = "od: ";
  append_row(out, first);
  out += " and ";
  append_row(out, second);
  out += ' ';
  out += what;
  return out;
}

constexpr std::string group_error(od_group const& group, std::string_view what)
{
  std::string out = "od: group '";
  out += group.name;
  out += "': ";
  out += what;
  return out;
}

constexpr std::string item_error(od_catalog const& catalog,
                                 std::size_t item,
                                 std::string_view what)
{
  std::string out = "od: ";
  out += catalog.what;
  out += " '";
  out += catalog.names[item];
  out += "' ";
  out += what;
  return out;
}

// Renders a PDO as "TPDO1 drive_status", or as "TPDO1" if it has no name.
constexpr void append_pdo(std::string& out, od_pdo const& pdo)
{
  out += pdo.direction == pdo_direction::tx ? "TPDO" : "RPDO";
  append_decimal(out, pdo.number);
  if (!pdo.name.empty()) {
    out += ' ';
    out += pdo.name;
  }
}

constexpr std::string pdo_error(od_pdo const& pdo, std::string_view what)
{
  std::string out = "od: ";
  append_pdo(out, pdo);
  out += ": ";
  out += what;
  return out;
}

constexpr std::string pdo_pair_error(od_pdo const& first,
                                     od_pdo const& second,
                                     std::string_view what)
{
  std::string out = "od: ";
  append_pdo(out, first);
  out += " and ";
  append_pdo(out, second);
  out += ' ';
  out += what;
  return out;
}

// The error of the signal of `pdo` that starts at bit `start`, named by the
// bit, as `pdo_check` names it: its path may be what is wrong.
constexpr std::string signal_error(od_pdo const& pdo,
                                   unsigned start,
                                   std::string_view what)
{
  return pdo_error(pdo, signal_name(start) + ": " + std::string{what});
}

constexpr std::string_view type_name(od_data_type type)
{
  switch (type) {
  case od_data_type::boolean: return "boolean";
  case od_data_type::int8: return "int8";
  case od_data_type::int16: return "int16";
  case od_data_type::int32: return "int32";
  case od_data_type::uint8: return "uint8";
  case od_data_type::uint16: return "uint16";
  case od_data_type::uint32: return "uint32";
  case od_data_type::float32: return "float32";
  }
  return "?";
}

constexpr std::string_view class_name(od_class class_)
{
  switch (class_) {
  case od_class::about: return "about";
  case od_class::control: return "control";
  case od_class::parameters: return "parameters";
  case od_class::status: return "status";
  case od_class::watch: return "watch";
  }
  return "?";
}

constexpr std::string_view access_name(od_access access)
{
  switch (access) {
  case od_access::rw: return "rw";
  case od_access::ro: return "ro";
  case od_access::wo: return "wo";
  case od_access::const_: return "const";
  }
  return "?";
}

constexpr std::string_view direction_name(pdo_direction direction)
{
  switch (direction) {
  case pdo_direction::tx: return "tx";
  case pdo_direction::rx: return "rx";
  }
  return "?";
}

constexpr std::string_view mapping_name(pdo_mapping mapping)
{
  switch (mapping) {
  case pdo_mapping::signals: return "signals";
  case pdo_mapping::sam: return "sam";
  }
  return "?";
}

constexpr std::string_view role_name(pdo_role role)
{
  switch (role) {
  case pdo_role::value: return "value";
  case pdo_role::counter: return "counter";
  }
  return "?";
}

constexpr std::string_view encoding_name(pdo_encoding encoding)
{
  switch (encoding) {
  case pdo_encoding::unsigned_: return "unsigned";
  case pdo_encoding::signed_: return "signed";
  }
  return "?";
}

// The classes of objects, each once: a group or a PDO named as one is
// refused, as a host would take its name for the class. The list is written
// out, so a class added to `od_class` must be added here too, or the rules
// that read the list miss it.
inline constexpr od_class od_classes[] = {od_class::about,
                                          od_class::control,
                                          od_class::parameters,
                                          od_class::status,
                                          od_class::watch};

// Whether `text` is a segment of a path: a letter followed by letters, digits
// and underscores, all ASCII.
constexpr bool is_segment(std::string_view text)
{
  auto const letter = [](char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
  };
  if (text.empty() || !letter(text.front())) return false;
  return std::ranges::all_of(text, [&](char c) {
    return letter(c) || (c >= '0' && c <= '9') || c == '_';
  });
}

// Whether `path` is a group and a name, two segments joined by a dot.
constexpr bool is_path(std::string_view path)
{
  auto const dot = path.find('.');
  return dot != std::string_view::npos
      && is_segment(path.substr(0, dot))
      && is_segment(path.substr(dot + 1));
}

// Returns the group of `path`, the part before its first dot.
constexpr std::string_view group_of(std::string_view path)
{
  return path.substr(0, path.find('.'));
}

// Whether `text` is valid UTF-8 without a quotation mark, a backslash or a
// control character (U+0000 to U+001F and U+007F to U+009F): a text that a
// string of JSON holds as it is, without escapes. Valid UTF-8 encodes each
// character in its shortest form, and no surrogate or code point past
// U+10FFFF.
constexpr bool is_verbatim_text(std::string_view text)
{
  constexpr std::uint32_t least[] = {0, 0, 0x80, 0x800, 0x10000};
  for (auto i = 0uz; i < text.size();) {
    auto const lead = static_cast<std::uint8_t>(text[i]);
    if (lead < 0x80) {
      if (lead < 0x20 || lead == 0x7F || lead == '"' || lead == '\\') {
        return false;
      }
      ++i;
      continue;
    }
    auto const length = lead < 0x80             ? 1uz
                      : (lead & 0xE0u) == 0xC0u ? 2uz
                      : (lead & 0xF0u) == 0xE0u ? 3uz
                      : (lead & 0xF8u) == 0xF0u ? 4uz
                                                : 0uz;
    if (length == 0 || i + length > text.size()) return false;
    std::uint32_t code = lead & (0x7Fu >> (length == 1 ? 0 : length));
    for (auto k = 1uz; k < length; ++k) {
      auto const next = static_cast<std::uint8_t>(text[i + k]);
      if ((next & 0xC0u) != 0x80u) return false;
      code = (code << 6) | (next & 0x3Fu);
    }
    if (code < least[length]
        || (code >= 0xD800 && code <= 0xDFFF)
        || code > 0x10FFFF) {
      return false;
    }
    if (code < 0x20 || (code >= 0x7F && code <= 0x9F)) return false;
    if (code == '"' || code == '\\') return false;
    i += length;
  }
  return true;
}

// Returns the rule that a title breaks, or an empty view if it breaks none:
// each language must have a text, and each text must be verbatim
// (`is_verbatim_text`).
constexpr std::string_view title_error(i18n const& title)
{
  if (title.en.empty() || title.ru.empty()) {
    return "the title must not be empty in any language";
  }
  if (!is_verbatim_text(title.en) || !is_verbatim_text(title.ru)) {
    return "the title must be UTF-8 without quotation marks, backslashes or "
           "control characters";
  }
  return {};
}

// Returns the area of `areas` that `index` lies in, or nullptr if it lies in
// none.
template<std::size_t A>
constexpr od_area const* area_of(std::uint16_t index, od_area const (&areas)[A])
{
  auto const area = std::ranges::find_if(areas, [&](od_area const& a) {
    return a.first <= index && index <= a.last;
  });
  return area != std::end(areas) ? area : nullptr;
}

// Returns the position in `groups` of the group named `name`, or `G` if no
// group has that name.
template<std::size_t G>
constexpr std::size_t group_position(std::string_view name,
                                     od_group const (&groups)[G])
{
  return static_cast<std::size_t>(
      std::ranges::find(groups, name, &od_group::name) - std::begin(groups));
}

// Returns the rule that an object breaks whose access is `access`, and that
// has a reader if `reader` and a writer if `writer`, or an empty view if it
// breaks none. A row that binds one object, a field of a record and the
// elements of an array obey the same rules.
constexpr std::string_view handler_error(od_access access,
                                         bool reader,
                                         bool writer)
{
  bool const readable = od_readable(access);
  bool const writable = od_writable(access);

  if (readable && !reader) return "readable but has no reader";
  if (!readable && reader) return "write-only but has a reader";
  if (writable && !writer) return "writable but has no writer";
  if (!writable && writer) return "read-only but has a writer";
  return {};
}

// Whether a member of the record `binding` inherits its value.
template<typename Ctx>
constexpr bool inherits_value(od_binding<Ctx> const& binding)
{
  for (auto const& label : binding.field_labels) {
    if (label.inherits) return true;
  }
  return false;
}

// Returns "value -1" for the value -1 of a table of values.
constexpr std::string value_name(std::int64_t value)
{
  std::string out = "value ";
  append_value(out, value);
  return out;
}

// Returns "values 0 and 2" for the values 0 and 2 of a table of values.
constexpr std::string values_name(std::int64_t first, std::int64_t second)
{
  std::string out = "values ";
  append_value(out, first);
  out += " and ";
  append_value(out, second);
  return out;
}

// Whether `value` is a value of the integer type that `type` names. No value
// is one of a `boolean` or a `float32`, which have no table of values.
constexpr bool fits(od_data_type type, std::int64_t value)
{
  switch (type) {
  case od_data_type::int8: return std::in_range<std::int8_t>(value);
  case od_data_type::int16: return std::in_range<std::int16_t>(value);
  case od_data_type::int32: return std::in_range<std::int32_t>(value);
  case od_data_type::uint8: return std::in_range<std::uint8_t>(value);
  case od_data_type::uint16: return std::in_range<std::uint16_t>(value);
  case od_data_type::uint32: return std::in_range<std::uint32_t>(value);
  case od_data_type::boolean:
  case od_data_type::float32: return false;
  }
  return false;
}

// Returns the rule that a table of values breaks by being given to an object
// or a member of type `type`, a command if `command` and an array of
// characters if `text`, or an empty view if it breaks none: the values of a
// command are not used, a character is no choice, a boolean has its own two
// values, and a float32 is no integer.
constexpr std::string_view table_error(od_data_type type,
                                       bool command,
                                       bool text)
{
  if (command) return "a command has no table of values";
  if (text) return "an array of characters has no table of values";
  if (type == od_data_type::boolean) return "a boolean has no table of values";
  if (type == od_data_type::float32) return "a float32 has no table of values";
  return {};
}

// Returns the rule that `values`, a table of values, breaks by being given
// to an object or a member of type `type`, or an empty string if it breaks
// none: each value is one of the type.
constexpr std::string values_fit_error(std::span<od_value_label const> values,
                                       od_data_type type)
{
  for (auto const& label : values) {
    if (!fits(type, label.value)) {
      return value_name(label.value)
           + " does not fit "
           + std::string{type_name(type)};
    }
  }
  return {};
}

// Returns the rule that `values`, a table of values, breaks, or an empty
// string if it breaks none: each value is named as a segment of a path and
// titled, the values strictly increase, and no two of them have the same
// name, or the same title in one language; in two languages a title may be
// the same, as an abbreviation is. The rules depend on the table alone, so a
// table that objects or members share is checked once
// (`check_value_tables`).
constexpr std::string value_labels_error(std::span<od_value_label const> values)
{
  for (auto i = 0uz; i < values.size(); ++i) {
    auto const& label = values[i];
    if (!is_segment(label.name)) {
      return value_name(label.value)
           + ": the name must be a letter followed by letters, digits or "
             "underscores";
    }
    if (auto const what = title_error(label.title); !what.empty()) {
      return value_name(label.value) + ": " + std::string{what};
    }
    if (i > 0 && values[i - 1].value >= label.value) {
      return value_name(label.value)
           + " follows "
           + value_name(values[i - 1].value)
           + ": the values must increase";
    }
    for (auto j = 0uz; j < i; ++j) {
      auto const& other = values[j];
      if (other.name == label.name) {
        return values_name(other.value, label.value) + " have the same name";
      }
      if (other.title.en == label.title.en) {
        return values_name(other.value, label.value)
             + " have the same title in English";
      }
      if (other.title.ru == label.title.ru) {
        return values_name(other.value, label.value)
             + " have the same title in Russian";
      }
    }
  }
  return {};
}

// Returns "member 03h" for the member at subindex `sub`.
constexpr std::string member_name(std::size_t sub)
{
  std::string out = "member ";
  append_hex(out, static_cast<unsigned>(sub), 2);
  out += 'h';
  return out;
}

// Returns the rule that `labels`, the labels of the members of a layout,
// break, or an empty string if they break none: each member is named as a
// segment of a path, unique in the layout, and titled, and only a command has
// a signature. The rules depend on the labels alone, so labels that records
// share are checked once (`check_shared_labels`).
constexpr std::string field_labels_error(std::span<od_field_label const> labels)
{
  for (auto i = 0uz; i < labels.size(); ++i) {
    auto const& label = labels[i];
    if (!is_segment(label.name)) {
      return member_name(i)
           + ": the name must be a letter followed by letters, digits or "
             "underscores";
    }
    for (auto j = 0uz; j < i; ++j) {
      if (labels[j].name == label.name) {
        std::string out = "members ";
        append_hex(out, static_cast<unsigned>(j), 2);
        out += "h and ";
        append_hex(out, static_cast<unsigned>(i), 2);
        out += "h have the same name";
        return out;
      }
    }
    if (auto const what = title_error(label.title); !what.empty()) {
      return member_name(i) + ": " + std::string{what};
    }
    if (label.signature && !label.command) {
      return member_name(i) + " has a signature but is no command";
    }
  }
  return {};
}

// The rules of the labels of the members of a record that depend on the
// record: a label for each field, a member that inherits the record's value
// of the record's type, no unit for a record without such a member, which the
// unit would not apply to, and a table of values only for a member that
// inherits nothing, whose type takes one (`table_error`) and holds each of
// its values. A member that inherits the record's value takes the record's
// table, and the unit of a member is only ever the record's.
template<typename Ctx>
constexpr std::string check_field_labels(od_row<Ctx> const& row)
{
  auto const& binding = row.binding;
  auto const& labels = binding.field_labels;

  if (labels.size() != binding.layout.size()) {
    std::string what = "the record has ";
    append_decimal(what, binding.layout.size());
    what += " members but labels ";
    append_decimal(what, labels.size());
    return row_error(row, what);
  }
  bool inherited = false;
  for (auto i = 0uz; i < labels.size(); ++i) {
    if (labels[i].inherits && binding.layout[i].type != binding.type) {
      std::string what = member_name(i) + " inherits the record's type, ";
      what += type_name(binding.type);
      what += ", but is ";
      what += type_name(binding.layout[i].type);
      return row_error(row, what);
    }
    inherited = inherited || labels[i].inherits;
  }
  if (!inherited && !unit_of(row).empty()) {
    return row_error(row,
                     "a record without a member that inherits its value has "
                     "no unit");
  }
  for (auto i = 0uz; i < labels.size(); ++i) {
    auto const& values = labels[i].value_labels;
    if (values.empty()) continue;
    if (labels[i].inherits) {
      return row_error(row,
                       member_name(i)
                           + " inherits the record's values and has no table "
                             "of its own");
    }
    auto const type = binding.layout[i].type;
    if (auto const what = table_error(type, labels[i].command, false);
        !what.empty()) {
      return row_error(row, member_name(i) + ": " + std::string{what});
    }
    if (auto const what = values_fit_error(values, type); !what.empty()) {
      return row_error(row, member_name(i) + ": " + what);
    }
  }
  return {};
}

// Checks the labels of the members of the records by `field_labels_error`,
// once for each set of labels, which the records of one kind share, and
// reports a broken one at the first record that has it.
template<typename Ctx, std::size_t N>
constexpr std::string check_shared_labels(od_row<Ctx> const (&rows)[N])
{
  std::array<std::span<od_field_label const>, N> checked{};
  auto count = 0uz;
  for (auto const& row : rows) {
    auto const& labels = row.binding.field_labels;
    if (row.binding.layout.empty()) continue;
    bool const seen = std::ranges::any_of(
        checked.begin(),
        checked.begin() + count,
        [&](std::span<od_field_label const> other) {
          return other.data() == labels.data() && other.size() == labels.size();
        });
    if (seen) continue;
    checked[count++] = labels;
    if (auto const what = field_labels_error(labels); !what.empty()) {
      return row_error(row, what);
    }
  }
  return {};
}

// Checks the tables of values by `value_labels_error`, once for each table,
// which rows and the labels of members may share, and reports a broken one at
// the first row that has it, and at its member. A table is told apart from
// another by its address, and the tables of the members of records that
// share their labels are visited once.
template<typename Ctx, std::size_t N>
constexpr std::string check_value_tables(od_row<Ctx> const (&rows)[N])
{
  std::vector<std::span<od_value_label const>> checked;
  std::vector<std::span<od_field_label const>> visited;
  auto const first_time = [&](std::span<od_value_label const> values) {
    bool const seen = std::ranges::any_of(
        checked,
        [&](std::span<od_value_label const> other) {
          return other.data() == values.data() && other.size() == values.size();
        });
    if (!seen) checked.push_back(values);
    return !seen;
  };

  for (auto const& row : rows) {
    auto const& values = row.binding.value_labels;
    if (!values.empty() && first_time(values)) {
      if (auto const what = value_labels_error(values); !what.empty()) {
        return row_error(row, what);
      }
    }

    auto const& labels = row.binding.field_labels;
    if (labels.empty()) continue;
    bool const seen = std::ranges::any_of(
        visited,
        [&](std::span<od_field_label const> other) {
          return other.data() == labels.data() && other.size() == labels.size();
        });
    if (seen) continue;
    visited.push_back(labels);
    for (auto i = 0uz; i < labels.size(); ++i) {
      auto const& member_values = labels[i].value_labels;
      if (member_values.empty() || !first_time(member_values)) continue;
      if (auto const what = value_labels_error(member_values); !what.empty()) {
        return row_error(row, member_name(i) + ": " + what);
      }
    }
  }
  return {};
}

// The rules of a row that binds a record: the row names the record with
// subindex 00h, the binding has no handlers of its own, the layout fits in the
// subindices, each field obeys the rules of one object, and the labels of
// the fields obey `check_field_labels`; `check_shared_labels` checks the rest
// of their rules once for all the records that share them.
template<typename Ctx>
constexpr std::string check_record(od_row<Ctx> const& row)
{
  auto const& binding = row.binding;

  if (row.key.subindex != 0) {
    return row_error(row, "a record's row must have subindex 00");
  }
  if (binding.read != nullptr || binding.write != nullptr) {
    return row_error(row, "a record has no handlers of its own");
  }
  if (binding.layout.size() > 0x100) {
    return row_error(row, "a record has at most 256 members");
  }
  for (auto i = 0uz; i < binding.layout.size(); ++i) {
    auto const& field = binding.layout[i];
    auto const what = handler_error(field.access,
                                    field.read != nullptr,
                                    field.write != nullptr);
    if (!what.empty()) {
      std::string out = "member ";
      append_hex(out, static_cast<unsigned>(i), 2);
      out += "h ";
      out += what;
      return row_error(row, out);
    }
  }
  return check_field_labels(row);
}

// The rules of a row that binds an array: the row names the array with
// subindex 00h, and its elements obey the rules of one object.
template<typename Ctx>
constexpr std::string check_array(od_row<Ctx> const& row)
{
  auto const& binding = row.binding;

  if (row.key.subindex != 0) {
    return row_error(row, "an array's row must have subindex 00");
  }
  auto const what = handler_error(binding.access,
                                  binding.read != nullptr,
                                  binding.write != nullptr);
  if (!what.empty()) {
    std::string out = "element ";
    out += what;
    return row_error(row, out);
  }
  return {};
}

// The rules of the label of a row: the row has none if its binding gives
// one, the path is a group and a name, the group is declared, and the title
// obeys `title_error`.
template<typename Ctx, std::size_t G>
constexpr std::string check_label(od_row<Ctx> const& row,
                                  od_group const (&groups)[G])
{
  auto const& label = label_of(row);

  if (!is_empty(row.binding.label) && !is_empty(row.label)) {
    return row_error(row, "the row has a label, but its binding gives one");
  }
  if (!is_path(label.path)) {
    return row_error(row,
                     "the path must be a group and a name, each a letter "
                     "followed by letters, digits or underscores");
  }
  if (group_position(group_of(label.path), groups) == G) {
    std::string what = "group '";
    what += group_of(label.path);
    what += "' is not declared";
    return row_error(row, what);
  }
  if (auto const what = title_error(label.title); !what.empty()) {
    return row_error(row, what);
  }
  return {};
}

// The rules of the unit of a row: it is verbatim (`is_verbatim_text`), the
// label does not repeat the unit the binding gives, the symbol of the
// value's type, and a command, an array of characters or a boolean has none:
// the value of a command is not used, a character is no quantity, and a
// boolean has no dimension.
template<typename Ctx>
constexpr std::string check_unit(od_row<Ctx> const& row)
{
  auto const& binding = row.binding;
  auto const unit = unit_of(row);

  if (unit.empty()) return {};
  if (!is_verbatim_text(unit)) {
    return row_error(row,
                     "the unit must be UTF-8 without quotation marks, "
                     "backslashes or control characters");
  }
  if (label_of(row).unit == binding.unit) {
    std::string what = "the unit '";
    what += unit;
    what += "' repeats the symbol of the value's type";
    return row_error(row, what);
  }
  if (binding.command) return row_error(row, "a command has no unit");
  if (binding.text) return row_error(row, "an array of characters has no unit");
  if (binding.type == od_data_type::boolean) {
    return row_error(row, "a boolean has no unit");
  }
  return {};
}

// The rules of the table of values of a row: an object, an array or a record
// whose type takes one (`table_error`), a record only if a member inherits
// its value, which is what the table names, no unit, which a named value does
// not need, and each value one of the type. The rules of the table alone are
// checked once for each table (`check_value_tables`).
template<typename Ctx>
constexpr std::string check_values(od_row<Ctx> const& row)
{
  auto const& binding = row.binding;
  auto const& values = binding.value_labels;

  if (values.empty()) return {};
  if (auto const what =
          table_error(binding.type, binding.command, binding.text);
      !what.empty()) {
    return row_error(row, what);
  }
  if (!binding.layout.empty() && !inherits_value(binding)) {
    return row_error(row,
                     "a record without a member that inherits its value has "
                     "no table of values");
  }
  if (!unit_of(row).empty()) {
    return row_error(row, "values that a table names have no unit");
  }
  if (auto const what = values_fit_error(values, binding.type); !what.empty()) {
    return row_error(row, what);
  }
  return {};
}

template<typename Ctx, std::size_t A, std::size_t G>
constexpr std::string check_row(od_row<Ctx> const& row,
                                od_area const (&areas)[A],
                                od_group const (&groups)[G])
{
  auto const& binding = row.binding;

  if (row.key.index < 0x1000) {
    return row_error(row, "indices below 1000h are reserved");
  }
  if (area_of(row.key.index, areas) == nullptr) {
    return row_error(row, "the index lies in no area");
  }
  if (auto error = check_label(row, groups); !error.empty()) return error;
  if (auto error = check_unit(row); !error.empty()) return error;
  if (auto error = check_values(row); !error.empty()) return error;

  if (!binding.layout.empty() && binding.count != nullptr) {
    return row_error(row, "a binding is a record or an array, not both");
  }
  if (!binding.layout.empty()) {
    if (auto error = check_record(row); !error.empty()) return error;
  }
  else if (binding.count != nullptr) {
    if (auto error = check_array(row); !error.empty()) return error;
  }
  else if (auto const what = handler_error(binding.access,
                                           binding.read != nullptr,
                                           binding.write != nullptr);
           !what.empty()) {
    return row_error(row, what);
  }
  if (binding.catalog != nullptr
      && binding.arg >= binding.catalog->names.size()) {
    return row_error(row, "arg is outside its catalog");
  }
  return {};
}

// Checks the areas: each ends no earlier than it begins, and no two overlap,
// which neighbors among the areas sorted by their first index would.
template<std::size_t A>
constexpr std::string check_areas(od_area const (&areas)[A])
{
  for (auto const& area : areas) {
    if (area.first > area.last) {
      std::string out = "od: area ";
      append_area(out, area);
      out += ": the first index is past the last";
      return out;
    }
  }

  std::array<od_area, A> sorted{};
  std::ranges::copy(areas, sorted.begin());
  std::ranges::sort(sorted, {}, &od_area::first);
  for (auto k = 1uz; k < A; ++k) {
    if (sorted[k].first <= sorted[k - 1].last) {
      std::string out = "od: areas ";
      append_area(out, sorted[k - 1]);
      out += " and ";
      append_area(out, sorted[k]);
      out += " overlap";
      return out;
    }
  }
  return {};
}

// Checks the groups: each name is a segment of a path, not the name of a
// class, and declared once, and each title obeys `title_error`.
template<std::size_t G>
constexpr std::string check_groups(od_group const (&groups)[G])
{
  for (auto i = 0uz; i < G; ++i) {
    auto const& group = groups[i];
    if (!is_segment(group.name)) {
      return group_error(group,
                         "the name must be a letter followed by letters, "
                         "digits or underscores");
    }
    if (std::ranges::contains(od_classes, group.name, class_name)) {
      return group_error(group, "named as a class");
    }
    if (group_position(group.name, groups) != i) {
      return group_error(group, "declared twice");
    }
    if (auto const what = title_error(group.title); !what.empty()) {
      return group_error(group, what);
    }
  }
  return {};
}

// Checks that each group has a row.
template<typename Ctx, std::size_t N, std::size_t G>
constexpr std::string check_groups_used(od_row<Ctx> const (&rows)[N],
                                        od_group const (&groups)[G])
{
  std::array<bool, G> used{};
  for (auto const& row : rows) {
    auto const group = group_position(group_of(label_of(row).path), groups);
    if (group < G) used[group] = true;
  }
  for (auto i = 0uz; i < G; ++i) {
    if (!used[i]) return group_error(groups[i], "has no row");
  }
  return {};
}

// Returns the positions of `rows` in the order that `less` puts the rows in,
// rows that `less` does not tell apart in the order of `rows`.
template<typename Ctx, std::size_t N, typename Less>
constexpr std::array<std::size_t, N>
sorted_positions(od_row<Ctx> const (&rows)[N], Less less)
{
  std::array<std::size_t, N> order{};
  for (auto i = 0uz; i < N; ++i) {
    order[i] = i;
  }
  std::ranges::sort(order, [&](std::size_t a, std::size_t b) {
    return less(rows[a], rows[b]) || (!less(rows[b], rows[a]) && a < b);
  });
  return order;
}

// Checks, over the rows sorted by key, that keys are unique, that a record or
// an array has its index to itself, and that no object at subindex 00h shares
// its index, whose subindex 00h the dictionary then serves. A row at 00h
// comes first among the rows of its index, next to any other.
template<typename Ctx, std::size_t N>
constexpr std::string check_keys(od_row<Ctx> const (&rows)[N])
{
  auto const order = sorted_positions(rows, [](auto const& a, auto const& b) {
    return a.key < b.key;
  });
  for (auto k = 1uz; k < N; ++k) {
    auto const& first = rows[std::min(order[k - 1], order[k])];
    auto const& second = rows[std::max(order[k - 1], order[k])];
    if (first.key.index != second.key.index) continue;

    if (!first.binding.layout.empty() || !second.binding.layout.empty()) {
      return pair_error(first, second, "share the index of a record");
    }
    if (first.binding.count != nullptr || second.binding.count != nullptr) {
      return pair_error(first, second, "share the index of an array");
    }
    if (first.key == second.key) {
      return pair_error(first, second, "have the same key");
    }
    if (first.key.subindex == 0 || second.key.subindex == 0) {
      return pair_error(first,
                        second,
                        "share an index whose subindex 00 the dictionary "
                        "serves");
    }
  }
  return {};
}

// Checks, over the rows sorted by path, that no two rows share a path.
template<typename Ctx, std::size_t N>
constexpr std::string check_paths(od_row<Ctx> const (&rows)[N])
{
  auto const order = sorted_positions(rows, [](auto const& a, auto const& b) {
    return label_of(a).path < label_of(b).path;
  });
  for (auto k = 1uz; k < N; ++k) {
    auto const& first = rows[std::min(order[k - 1], order[k])];
    auto const& second = rows[std::max(order[k - 1], order[k])];
    if (label_of(first).path == label_of(second).path) {
      return pair_error(first, second, "have the same path");
    }
  }
  return {};
}

// Checks that no two rows outside a catalog are served by the same handler
// with the same argument, which a copied row would be. Rows of a catalog share
// their handlers by design; a duplicate among them is reported per item. The
// handlers are compared in pairs: pointers to different functions have no
// order in constant evaluation, so they cannot be sorted.
template<typename Ctx, std::size_t N>
constexpr std::string check_handlers(od_row<Ctx> const (&rows)[N])
{
  std::array<std::size_t, N> plain{};
  auto count = 0uz;
  for (auto i = 0uz; i < N; ++i) {
    if (rows[i].binding.catalog == nullptr) plain[count++] = i;
  }

  for (auto p = 0uz; p < count; ++p) {
    for (auto q = p + 1; q < count; ++q) {
      auto const& first = rows[plain[p]];
      auto const& second = rows[plain[q]];
      auto const& a = first.binding;
      auto const& b = second.binding;
      if (a.arg != b.arg) continue;
      if (a.read != nullptr && a.read == b.read) {
        return pair_error(first, second, "are read by the same handler");
      }
      if (a.write != nullptr && a.write == b.write) {
        return pair_error(first, second, "are written by the same handler");
      }
    }
  }
  return {};
}

// Checks that each exposed item of `catalog` has exactly one row and each
// hidden one none.
template<typename Ctx, std::size_t N>
constexpr std::string check_coverage(od_row<Ctx> const (&rows)[N],
                                     od_catalog const& catalog)
{
  // The first two rows of each item; `check_row` keeps `arg` in the catalog.
  std::vector<od_row<Ctx> const*> first(catalog.names.size(), nullptr);
  std::vector<od_row<Ctx> const*> second(catalog.names.size(), nullptr);
  for (auto const& row : rows) {
    if (row.binding.catalog != &catalog) continue;
    auto const item = std::size_t{row.binding.arg};
    if (first[item] == nullptr) {
      first[item] = &row;
    }
    else if (second[item] == nullptr) {
      second[item] = &row;
    }
  }

  for (auto item = 0uz; item < catalog.names.size(); ++item) {
    if (catalog.exposed[item] && first[item] == nullptr) {
      return item_error(catalog, item, "has no row");
    }
    if (catalog.exposed[item] && second[item] != nullptr) {
      auto out = item_error(catalog, item, "has two rows: ");
      append_row(out, *first[item]);
      out += " and ";
      append_row(out, *second[item]);
      return out;
    }
    if (!catalog.exposed[item] && first[item] != nullptr) {
      auto out = item_error(catalog, item, "is not exposed but has a row: ");
      append_row(out, *first[item]);
      return out;
    }
  }
  return {};
}

// Checks the coverage of every catalog that a row serves an item of, once
// each, in the order of their first rows.
template<typename Ctx, std::size_t N>
constexpr std::string check_catalogs(od_row<Ctx> const (&rows)[N])
{
  std::vector<od_catalog const*> checked;
  for (auto const& row : rows) {
    od_catalog const* const catalog = row.binding.catalog;
    if (catalog == nullptr || std::ranges::contains(checked, catalog)) {
      continue;
    }
    checked.push_back(catalog);
    if (auto error = check_coverage(rows, *catalog); !error.empty()) {
      return error;
    }
  }
  return {};
}

// The rules of a PDO alone: its name is a segment of a path, the first
// segment of the paths of its signals, and names no group, whose objects a
// host would mix up with them, and no class; its title obeys `title_error`;
// its number is that of a PDO of CiA 301, 1 to 512, and only PDOs 1 to 4 have
// the predefined COB-ID, as `cob_id_of` gives it; a custom COB-ID is one of 11
// bits, from 001h to 7FFh, as `put_hex` writes three digits of it and would
// drop the others, and not restricted (`is_restricted_cob_id`); a TPDO has a
// period above 0, as a slot with none sends nothing, and no timeout, and an
// RPDO has no period and a timeout of 0, unwatched, or more.
template<std::size_t G>
constexpr std::string check_pdo(od_pdo const& pdo, od_group const (&groups)[G])
{
  constexpr auto zero = std::chrono::milliseconds::zero();
  auto const& id = pdo.cob_id;

  if (!is_segment(pdo.name)) {
    return pdo_error(pdo,
                     "the name must be a letter followed by letters, digits "
                     "or underscores");
  }
  if (group_position(pdo.name, groups) != G) {
    return pdo_error(pdo, "named as a group");
  }
  if (std::ranges::contains(od_classes, pdo.name, class_name)) {
    return pdo_error(pdo, "named as a class");
  }
  if (auto const what = title_error(pdo.title); !what.empty()) {
    return pdo_error(pdo, what);
  }
  if (pdo.number < 1 || pdo.number > 512) {
    return pdo_error(pdo, "the number must be from 1 to 512");
  }
  if (!id.is_custom && pdo.number > 4) {
    return pdo_error(pdo, "the predefined COB-ID exists only for PDOs 1 to 4");
  }
  if (id.is_custom && (id.value < 0x001 || id.value > 0x7FF)) {
    return pdo_error(pdo, "the COB-ID must be from 001h to 7FFh");
  }
  if (id.is_custom && is_restricted_cob_id(id.value)) {
    std::string what = "the COB-ID ";
    append_hex(what, id.value, 3);
    what += "h is restricted";
    return pdo_error(pdo, what);
  }
  if (pdo.direction == pdo_direction::tx) {
    if (pdo.timeout != zero) return pdo_error(pdo, "a TPDO has no timeout");
    if (pdo.period <= zero) {
      return pdo_error(pdo, "the period must be above 0");
    }
  }
  else {
    if (pdo.period != zero) return pdo_error(pdo, "an RPDO has no period");
    if (pdo.timeout < zero) {
      return pdo_error(pdo, "the timeout must not be negative");
    }
  }
  return {};
}

// The rules of the signals of a PDO that its rows do not check, as they need
// the name of the PDO and the rules of a description; the rows obey their
// own (`pdo_check`), which `pdo_of` checks. The path of a signal of the role
// `value` is the name of the PDO and a name, each a segment, and no other
// signal of the PDO has it: no path of a row has a PDO's name for its group,
// and PDOs have different names, so a path is unique in the whole
// description. Its title obeys `title_error`; a unit is verbatim
// (`is_verbatim_text`), and a step `scale_num` / `scale_den` a positive
// fraction in its lowest terms, as the description writes it, which that of
// an `emb::scaled` is unless its ratio is negative.
constexpr std::string check_signals(od_mapped_pdo const& mapped)
{
  auto const& pdo = mapped.pdo;
  auto const& signals = mapped.signals;

  for (auto i = 0uz; i < signals.size(); ++i) {
    auto const& signal = signals[i];
    auto const& label = signal.label;
    auto const start = unsigned{signal.bits.start};
    if (signal.role == pdo_role::value) {
      if (!is_path(label.path) || group_of(label.path) != pdo.name) {
        return signal_error(pdo,
                            start,
                            "the path must be the name of the PDO and a name, "
                            "each a letter followed by letters, digits or "
                            "underscores");
      }
      if (auto const what = title_error(label.title); !what.empty()) {
        return signal_error(pdo, start, what);
      }
      for (auto j = 0uz; j < i; ++j) {
        if (signals[j].role == pdo_role::value
            && signals[j].label.path == label.path) {
          return pdo_error(pdo,
                           signal_name(signals[j].bits.start)
                               + " and "
                               + signal_name(start)
                               + " have the same path");
        }
      }
    }
    if (!is_verbatim_text(signal.unit)) {
      return signal_error(pdo,
                          start,
                          "the unit must be UTF-8 without quotation marks, "
                          "backslashes or control characters");
    }
    if (signal.scale_num <= 0
        || signal.scale_den <= 0
        || std::gcd(signal.scale_num, signal.scale_den) != 1) {
      return signal_error(pdo,
                          start,
                          "the step must be a positive fraction in its lowest "
                          "terms");
    }
  }
  return {};
}

// Checks the tables of values of the signals by `value_labels_error`, once
// for each table, which signals may share with each other and with the rows,
// and reports a broken one at the first signal that has it.
constexpr std::string check_signal_tables(std::span<od_mapped_pdo const> pdos)
{
  std::vector<std::span<od_value_label const>> checked;
  for (auto const& mapped : pdos) {
    for (auto const& signal : mapped.signals) {
      auto const& values = signal.value_labels;
      if (values.empty()) continue;
      bool const seen =
          std::ranges::any_of(checked,
                              [&](std::span<od_value_label const> other) {
                                return other.data() == values.data()
                                    && other.size() == values.size();
                              });
      if (seen) continue;
      checked.push_back(values);
      if (auto const what = value_labels_error(values); !what.empty()) {
        return signal_error(mapped.pdo, signal.bits.start, what);
      }
    }
  }
  return {};
}

// Whether the custom COB-ID `id` is one of the COB-IDs of the predefined form
// that `pdo` has: its base + a node-ID of 1 to 127.
constexpr bool in_predefined_form(id_t id, od_pdo const& pdo)
{
  if (pdo.cob_id.is_custom) return false;
  auto const base = cob_base_of(pdo.direction, pdo.number);
  return id > base && id <= base + node_id::max;
}

// Returns the rule that `first`, of a custom COB-ID, breaks with `second`, or
// an empty string: `first` has the COB-ID of `second`, custom, or that of the
// predefined form of `second` for some node-ID.
constexpr std::string cob_id_error(od_pdo const& first, od_pdo const& second)
{
  auto const id = first.cob_id.value;
  bool const custom = second.cob_id.is_custom;
  if (!first.cob_id.is_custom
      || (custom && second.cob_id.value != id)
      || (!custom && !in_predefined_form(id, second))) {
    return {};
  }
  std::string what = "share COB-ID ";
  append_hex(what, id, 3);
  what += 'h';
  if (!custom) {
    what += " on node ";
    append_decimal(what, id - cob_base_of(second.direction, second.number));
  }
  return pdo_pair_error(first, second, what);
}

// The rules of two PDOs, in pairs, in source order: no two go in the same
// direction with the same number, as two would set one slot up, or have the
// same name, the first segment of the paths of their signals; and no two
// share a COB-ID. A custom COB-ID is no other's custom one, and none of the
// COB-IDs of the predefined form of any PDO of the description, of either
// direction, its base + a node-ID: the node-ID is known at run time only.
constexpr std::string check_pdo_pairs(std::span<od_mapped_pdo const> pdos)
{
  for (auto p = 0uz; p < pdos.size(); ++p) {
    for (auto q = p + 1; q < pdos.size(); ++q) {
      auto const& first = pdos[p].pdo;
      auto const& second = pdos[q].pdo;
      if (first.direction == second.direction
          && first.number == second.number) {
        return pdo_pair_error(first, second, "have the same number");
      }
      if (first.name == second.name) {
        return pdo_pair_error(first, second, "have the same name");
      }
      if (auto error = cob_id_error(first, second); !error.empty()) {
        return error;
      }
      if (auto error = cob_id_error(second, first); !error.empty()) {
        return error;
      }
    }
  }
  return {};
}

// Returns the first rule that the process data breaks, or an empty string:
// the rules of each PDO alone and of its signals, in source order, then those
// of the tables of values of the signals, once for each table, and last
// those of two PDOs.
template<std::size_t G>
constexpr std::string check_process_data(od_process_data const& process_data,
                                         od_group const (&groups)[G])
{
  for (auto const& mapped : process_data.pdos) {
    if (auto error = check_pdo(mapped.pdo, groups); !error.empty()) {
      return error;
    }
    if (auto error = check_signals(mapped); !error.empty()) return error;
  }
  if (auto error = check_signal_tables(process_data.pdos); !error.empty()) {
    return error;
  }
  return check_pdo_pairs(process_data.pdos);
}

// Returns an empty string if the rows, the areas and the groups make a valid
// dictionary, and `process_data` valid PDOs beside it, or else the first rule
// they break: rules of the areas, then of the groups, then of a single row in
// source order, then those of the labels of the members of records, once for
// each set of labels, then those of the tables of values, once for each
// table, then those of two rows, keys before paths before handlers, then
// catalog coverage, then that every group has a row, and last the rules of
// the process data (`check_process_data`). Returns an empty string as well if
// a builder has already reported an error in some row. Keys and paths are
// checked over sorted rows, catalogs in one pass each; only the rows outside
// a catalog are compared in pairs, for their handlers, and the PDOs, which
// are few.
template<typename Ctx, std::size_t N, std::size_t A, std::size_t G>
constexpr std::string od_check(od_row<Ctx> const (&rows)[N],
                               od_area const (&areas)[A],
                               od_group const (&groups)[G],
                               od_process_data const& process_data = {})
{
  for (auto const& row : rows) {
    if (row.binding.diagnosed) return {};
  }
  if (auto error = check_areas(areas); !error.empty()) return error;
  if (auto error = check_groups(groups); !error.empty()) return error;
  for (auto const& row : rows) {
    if (auto error = check_row(row, areas, groups); !error.empty()) {
      return error;
    }
  }
  if (auto error = check_shared_labels(rows); !error.empty()) return error;
  if (auto error = check_value_tables(rows); !error.empty()) return error;
  if (auto error = check_keys(rows); !error.empty()) return error;
  if (auto error = check_paths(rows); !error.empty()) return error;
  if (auto error = check_handlers(rows); !error.empty()) return error;
  if (auto error = check_catalogs(rows); !error.empty()) return error;
  if (auto error = check_groups_used(rows, groups); !error.empty()) {
    return error;
  }
  return check_process_data(process_data, groups);
}

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
