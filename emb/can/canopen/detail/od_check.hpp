#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "../od.hpp"

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

// The rules of a row that binds a record: the row names the record with
// subindex 00h, the binding has no handlers of its own, the layout fits in the
// subindices, and each field obeys the rules of one object.
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
  return {};
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
  constexpr od_class classes[] = {od_class::about,
                                  od_class::control,
                                  od_class::parameters,
                                  od_class::watch};
  for (auto i = 0uz; i < G; ++i) {
    auto const& group = groups[i];
    if (!is_segment(group.name)) {
      return group_error(group,
                         "the name must be a letter followed by letters, "
                         "digits or underscores");
    }
    if (std::ranges::contains(classes, group.name, class_name)) {
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

// Returns an empty string if the rows, the areas and the groups make a valid
// dictionary, or else the first rule they break: rules of the areas, then of
// the groups, then of a single row in source order, then those of two rows,
// keys before paths before handlers, then catalog coverage, and last that
// every group has a row. Returns an empty string as well if a builder has
// already reported an error in some row. Keys and paths are checked over
// sorted rows, catalogs in one pass each; only the rows outside a catalog are
// compared in pairs, for their handlers.
template<typename Ctx, std::size_t N, std::size_t A, std::size_t G>
constexpr std::string od_check(od_row<Ctx> const (&rows)[N],
                               od_area const (&areas)[A],
                               od_group const (&groups)[G])
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
  if (auto error = check_keys(rows); !error.empty()) return error;
  if (auto error = check_paths(rows); !error.empty()) return error;
  if (auto error = check_handlers(rows); !error.empty()) return error;
  if (auto error = check_catalogs(rows); !error.empty()) return error;
  return check_groups_used(rows, groups);
}

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
