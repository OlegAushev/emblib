#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
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
  case od_data_type::exec: return "exec";
  case od_data_type::string: return "string";
  }
  return "?";
}

// Returns the rule that an object breaks whose access is `access` and type
// `type`, and that has a reader if `reader` and a writer if `writer`, or an
// empty view if it breaks none. A row that binds one object and a field of a
// record obey the same rules.
constexpr std::string_view
handler_error(od_access access, od_data_type type, bool reader, bool writer)
{
  bool const readable = od_readable(access);
  bool const writable = od_writable(access);

  if (readable && !reader) return "readable but has no reader";
  if (!readable && reader) return "write-only but has a reader";
  if (writable && !writer) return "writable but has no writer";
  if (!writable && writer) return "read-only but has a writer";
  if (type == od_data_type::exec && access != od_access::wo) {
    return "an exec object must be wo";
  }
  if (type == od_data_type::string && writable) {
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

// Checks, over the rows sorted by key, that keys are unique and that a record
// has its index to itself. A record's row has subindex 00h, so it comes first
// among the rows of its index, next to any other.
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
    if (first.key == second.key) {
      return pair_error(first, second, "have the same key");
    }
  }
  return {};
}

// Checks, over the rows sorted by name, that no two rows share category,
// subcategory and name.
template<typename Ctx, std::size_t N>
constexpr std::string check_names(od_row<Ctx> const (&rows)[N])
{
  auto const name_of = [](od_row<Ctx> const& row) {
    return std::tuple{row.category, row.subcategory, row.name};
  };
  auto const order = sorted_positions(rows, [&](auto const& a, auto const& b) {
    return name_of(a) < name_of(b);
  });
  for (auto k = 1uz; k < N; ++k) {
    auto const& first = rows[std::min(order[k - 1], order[k])];
    auto const& second = rows[std::max(order[k - 1], order[k])];
    if (name_of(first) == name_of(second)) {
      return pair_error(first, second, "have the same name");
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

// Returns an empty string if the rows make a valid dictionary, or else the
// first rule they break: rules of a single row in source order, then those of
// two rows, keys before names before handlers, then catalog coverage. Returns
// an empty string as well if a builder has already reported an error in some
// row. Keys and names are checked over sorted rows, catalogs in one pass each;
// only the rows outside a catalog are compared in pairs, for their handlers.
template<typename Ctx, std::size_t N>
constexpr std::string od_check(od_row<Ctx> const (&rows)[N])
{
  for (auto const& row : rows) {
    if (row.binding.diagnosed) return {};
  }
  for (auto const& row : rows) {
    if (auto error = check_row(row); !error.empty()) return error;
  }
  if (auto error = check_keys(rows); !error.empty()) return error;
  if (auto error = check_names(rows); !error.empty()) return error;
  if (auto error = check_handlers(rows); !error.empty()) return error;
  return check_catalogs(rows);
}

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
