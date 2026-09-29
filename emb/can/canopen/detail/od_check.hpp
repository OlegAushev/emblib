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

template<typename Ctx>
constexpr bool serves_restore_default(od_binding<Ctx> const& binding)
{
  return binding.access == od_access::wo
      && binding.type == od_value_type::exec
      && binding.read == nullptr
      && binding.write == nullptr
      && binding.restore == nullptr
      && binding.catalog == nullptr;
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

  if (row.key == od_restore_default_key) {
    if (!serves_restore_default(binding)) {
      return row_error(
          row,
          "the server serves this key; bind it with od_restore_default");
    }
    return {};
  }

  bool const readable = od_readable(binding.access);
  bool const writable = od_writable(binding.access);

  if (readable && binding.read == nullptr) {
    return row_error(row, "readable but has no reader");
  }
  if (!readable && binding.read != nullptr) {
    return row_error(row, "write-only but has a reader");
  }
  if (writable && binding.write == nullptr) {
    return row_error(row, "writable but has no writer");
  }
  if (!writable && binding.write != nullptr) {
    return row_error(row, "read-only but has a writer");
  }
  if (binding.type == od_value_type::exec && binding.access != od_access::wo) {
    return row_error(row, "an exec object must be wo");
  }
  if (binding.type == od_value_type::string && writable) {
    return row_error(row, "a string object must not be writable");
  }
  if (binding.restore != nullptr && !writable) {
    return row_error(row, "restorable but not writable");
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

      if (first.key == second.key) {
        return pair_error(first, second, "have the same key");
      }
      if (first.category == second.category
          && first.subcategory == second.subcategory
          && first.name == second.name) {
        return pair_error(first, second, "have the same name");
      }
      if (a.restore != nullptr
          && b.restore != nullptr
          && a.restore != b.restore) {
        return pair_error(first, second, "restore through different functions");
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
