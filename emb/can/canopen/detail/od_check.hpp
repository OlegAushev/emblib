#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

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

// Renders a row as "3002h:01 config/drive/phase_swap".
template<typename Ctx>
constexpr void append_row(std::string& out, od_row<Ctx> const& row)
{
  append_hex(out, row.key.index, 4);
  out += "h:";
  append_hex(out, row.key.subindex, 2);
  out += ' ';
  out += row.category;
  out += '/';
  out += row.subcategory;
  out += '/';
  out += row.name;
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

constexpr std::string_view type_name(od_value_type type)
{
  switch (type) {
  case od_value_type::boolean: return "boolean";
  case od_value_type::int8: return "int8";
  case od_value_type::int16: return "int16";
  case od_value_type::int32: return "int32";
  case od_value_type::uint8: return "uint8";
  case od_value_type::uint16: return "uint16";
  case od_value_type::uint32: return "uint32";
  case od_value_type::float32: return "float32";
  case od_value_type::exec: return "exec";
  case od_value_type::string: return "string";
  }
  return "?";
}

// Returns the rule that an object breaks whose access is `access` and type
// `type`, and that has a reader if `reader` and a writer if `writer`, or an
// empty view if it breaks none. A row that binds one object and a field of a
// record obey the same rules.
constexpr std::string_view
handler_error(od_access access, od_value_type type, bool reader, bool writer)
{
  bool const readable = od_readable(access);
  bool const writable = od_writable(access);

  if (readable && !reader) return "readable but has no reader";
  if (!readable && reader) return "write-only but has a reader";
  if (writable && !writer) return "writable but has no writer";
  if (!writable && writer) return "read-only but has a writer";
  if (type == od_value_type::exec && access != od_access::wo) {
    return "an exec object must be wo";
  }
  if (type == od_value_type::string && writable) {
    return "a string object must not be writable";
  }
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
                                    field.type,
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

template<typename Ctx>
constexpr std::string check_row(od_row<Ctx> const& row)
{
  auto const& binding = row.binding;

  if (row.key.index < 0x1000) {
    return row_error(row, "indices below 1000h are reserved");
  }
  if (row.category.empty() || row.subcategory.empty() || row.name.empty()) {
    return row_error(row, "category, subcategory and name must not be empty");
  }
  if (row.type != binding.type) {
    std::string what = "declared ";
    what += type_name(row.type);
    what += ", bound as ";
    what += type_name(binding.type);
    return row_error(row, what);
  }

  if (!binding.layout.empty()) {
    if (auto error = check_record(row); !error.empty()) return error;
  }
  else if (auto const what = handler_error(binding.access,
                                           binding.type,
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

template<typename Ctx, std::size_t N>
constexpr std::string check_pairs(od_row<Ctx> const (&rows)[N])
{
  for (auto i = 0uz; i < N; ++i) {
    for (auto j = i + 1; j < N; ++j) {
      auto const& first = rows[i];
      auto const& second = rows[j];
      auto const& a = first.binding;
      auto const& b = second.binding;

      bool const record = !a.layout.empty() || !b.layout.empty();
      if (record && (first.key.index == second.key.index)) {
        return pair_error(first, second, "share the index of a record");
      }
      if (first.key == second.key) {
        return pair_error(first, second, "have the same key");
      }
      if (first.category == second.category
          && first.subcategory == second.subcategory
          && first.name == second.name) {
        return pair_error(first, second, "have the same name");
      }
      // Rows of a catalog share their handlers by design; a duplicate
      // among them is reported per item below.
      if (a.catalog == nullptr && b.catalog == nullptr && a.arg == b.arg) {
        if (a.read != nullptr && a.read == b.read) {
          return pair_error(first, second, "are read by the same handler");
        }
        if (a.write != nullptr && a.write == b.write) {
          return pair_error(first, second, "are written by the same handler");
        }
      }
    }
  }
  return {};
}

template<typename Ctx, std::size_t N>
constexpr std::string check_catalogs(od_row<Ctx> const (&rows)[N])
{
  for (auto r = 0uz; r < N; ++r) {
    od_catalog const* const catalog = rows[r].binding.catalog;
    if (catalog == nullptr) continue;

    bool checked = false;
    for (auto p = 0uz; p < r; ++p) {
      if (rows[p].binding.catalog == catalog) checked = true;
    }
    if (checked) continue;

    for (auto item = 0uz; item < catalog->names.size(); ++item) {
      od_row<Ctx> const* first = nullptr;
      od_row<Ctx> const* second = nullptr;
      for (auto const& row : rows) {
        if (row.binding.catalog != catalog || row.binding.arg != item) {
          continue;
        }
        if (first == nullptr) {
          first = &row;
        }
        else if (second == nullptr) {
          second = &row;
        }
      }

      if (catalog->exposed[item] && first == nullptr) {
        return item_error(*catalog, item, "has no row");
      }
      if (catalog->exposed[item] && second != nullptr) {
        auto out = item_error(*catalog, item, "has two rows: ");
        append_row(out, *first);
        out += " and ";
        append_row(out, *second);
        return out;
      }
      if (!catalog->exposed[item] && first != nullptr) {
        auto out = item_error(*catalog, item, "is not exposed but has a row: ");
        append_row(out, *first);
        return out;
      }
    }
  }
  return {};
}

// Returns an empty string if the rows make a valid dictionary, or else the
// first rule they break: rules of a single row in source order, then those of
// two rows, then catalog coverage. Returns an empty string as well if a
// builder has already reported an error in some row.
template<typename Ctx, std::size_t N>
constexpr std::string od_check(od_row<Ctx> const (&rows)[N])
{
  for (auto const& row : rows) {
    if (row.binding.diagnosed) return {};
  }
  for (auto const& row : rows) {
    if (auto error = check_row(row); !error.empty()) return error;
  }
  if (auto error = check_pairs(rows); !error.empty()) return error;
  return check_catalogs(rows);
}

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
