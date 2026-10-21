#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include "detail/od_check.hpp"
#include "od.hpp"

namespace emb {
namespace can {
namespace canopen {

namespace detail {

template<typename T>
struct od_rows_traits {};

template<typename Ctx, std::size_t N>
struct od_rows_traits<od_row<Ctx> const[N]> {
  using context_type = Ctx;
  static constexpr std::size_t size = N;
};

// Returns the fingerprint of `rows`, whose indices lie in `areas`: the 32-bit
// FNV-1a hash of the text that describes them, a line for each row in the
// order of keys, as a host can write it from the rows of its own table. A
// row's line names its key and path as the diagnostics do, its class, its
// type, and whether it serves an object, an array or a record:
// "3000h:01 nvm.magic parameters uint32 object ro". An object's or an array's
// line ends with its access, an array's that of its elements; a record's line
// is followed by a line for each field of its layout, " 00h uint8 const".
// Every line ends in a newline. The title and the unit are left out: they
// change how a value reads, not where it is or how it travels.
template<typename Ctx, std::size_t N, std::size_t A>
constexpr std::uint32_t od_fingerprint(od_row<Ctx> const (&rows)[N],
                                       od_area const (&areas)[A])
{
  auto const order = sorted_positions(rows, [](auto const& a, auto const& b) {
    return a.key < b.key;
  });

  std::uint32_t h = 0x811C9DC5u;
  auto const mix = [&h](std::string_view text) {
    for (char c : text) {
      h ^= static_cast<std::uint8_t>(c);
      h *= 0x01000193u;
    }
  };

  std::string line;
  for (auto const i : order) {
    auto const& row = rows[i];
    auto const& binding = row.binding;
    line.clear();
    append_row(line, row);
    line += ' ';
    // A row outside every area breaks a rule of `od_check`, which reports it;
    // its line no longer matters.
    auto const* const area = area_of(row.key.index, areas);
    line += area != nullptr ? class_name(area->class_) : "?";
    line += ' ';
    line += type_name(binding.type);
    if (!binding.layout.empty()) {
      line += " record\n";
      for (auto f = 0uz; f < binding.layout.size(); ++f) {
        line += ' ';
        append_hex(line, static_cast<unsigned>(f), 2);
        line += "h ";
        line += type_name(binding.layout[f].type);
        line += ' ';
        line += access_name(binding.layout[f].access);
        line += '\n';
      }
    }
    else {
      line += binding.count != nullptr ? " array " : " object ";
      line += access_name(binding.access);
      line += '\n';
    }
    mix(line);
  }
  return h;
}

// Returns the number of indices whose objects, the rows of neither a record
// nor an array, all have a subindex past 00h: the indices whose subindex 00h
// the dictionary serves itself.
template<typename Ctx, std::size_t N>
constexpr std::size_t od_implied_count(od_row<Ctx> const (&rows)[N])
{
  std::array<od_key, N> keys{};
  auto count = 0uz;
  for (auto const& row : rows) {
    if (row.binding.layout.empty() && row.binding.count == nullptr) {
      keys[count++] = row.key;
    }
  }
  std::ranges::sort(keys.begin(), keys.begin() + count);

  auto implied = 0uz;
  for (auto i = 0uz; i < count; ++i) {
    bool const first = (i == 0) || (keys[i - 1].index != keys[i].index);
    if (first && keys[i].subindex != 0) ++implied;
  }
  return implied;
}

// Returns `arg`, the highest subindex of an index whose subindex 00h the
// dictionary serves itself.
template<typename Ctx>
constexpr od_read_result od_highest_subindex(Ctx&, std::uint16_t arg)
{
  return od_value{static_cast<std::uint8_t>(arg)};
}

} // namespace detail

// The concept `some_od_rows<T>` is satisfied if and only if `T` is a C array
// of `od_row<Ctx> const` for some `Ctx`.
template<typename T>
concept some_od_rows =
    requires { typename detail::od_rows_traits<T>::context_type; };

// The concept `some_od_areas<T>` is satisfied if and only if `T` is a C array
// of `od_area const`.
template<typename T>
concept some_od_areas = std::is_bounded_array_v<T>
                     && std::same_as<std::remove_extent_t<T>, od_area const>;

// The concept `some_od_groups<T>` is satisfied if and only if `T` is a C
// array of `od_group const`.
template<typename T>
concept some_od_groups = std::is_bounded_array_v<T>
                      && std::same_as<std::remove_extent_t<T>, od_group const>;

template<typename Ctx,
         std::size_t Entries,
         std::size_t Records,
         std::size_t Arrays>
class od_dictionary;

template<auto const& Rows, auto const& Areas, auto const& Groups>
  requires some_od_rows<std::remove_reference_t<decltype(Rows)>>
        && some_od_areas<std::remove_reference_t<decltype(Areas)>>
        && some_od_groups<std::remove_reference_t<decltype(Groups)>>
consteval auto make_dictionary();

// The class template `od_dictionary` holds the entries of a dictionary sorted
// by key, and its records and arrays sorted by index. Only `make_dictionary`
// creates a dictionary, after checking the rows it is made of, so every
// dictionary, and every `od_view` of one, meets the server's preconditions:
// keys are unique and sorted, a record or an array has its index to itself,
// and an entry, a field or the elements of an array have a reader exactly if
// their access is readable and a writer exactly if it is writable.
template<typename Ctx,
         std::size_t Entries,
         std::size_t Records,
         std::size_t Arrays>
class od_dictionary {
public:
  constexpr std::span<od_entry<Ctx> const> entries() const
  {
    return entries_;
  }

  constexpr std::span<od_record<Ctx> const> records() const
  {
    return records_;
  }

  constexpr std::span<od_array<Ctx> const> arrays() const
  {
    return arrays_;
  }

  // Returns the fingerprint of the rows the dictionary was made of, which a
  // host compares with that of its own table (`detail::od_fingerprint`).
  constexpr std::uint32_t fingerprint() const
  {
    return fingerprint_;
  }

private:
  consteval od_dictionary() = default;

  template<auto const& Rows, auto const& Areas, auto const& Groups>
    requires some_od_rows<std::remove_reference_t<decltype(Rows)>>
          && some_od_areas<std::remove_reference_t<decltype(Areas)>>
          && some_od_groups<std::remove_reference_t<decltype(Groups)>>
  friend consteval auto make_dictionary();

  std::array<od_entry<Ctx>, Entries> entries_{};
  std::array<od_record<Ctx>, Records> records_{};
  std::array<od_array<Ctx>, Arrays> arrays_{};
  std::uint32_t fingerprint_ = 0;
};

// Returns the dictionary made of `Rows`, a C array of `od_row<Ctx> const`,
// whose indices lie in `Areas`, a C array of `od_area const`, and whose paths
// start with the groups of `Groups`, a C array of `od_group const`. Each must
// be an `inline constexpr` variable of a named namespace: GCC at -O0 emits any
// other table, the labels in it included. A row whose binding has a layout
// becomes a record, one whose binding has a count an array, any other an
// entry. An index whose entries all have a subindex past 00h gets one more,
// at 00h: a `const` `uint8` that reads the highest subindex of the index, as
// CiA 301 asks of an object with members. The areas and the groups only
// check the rows and enter the fingerprint; the dictionary keeps neither. The
// program is ill-formed if the rows, the areas or the groups break a rule of
// `detail::od_check`; the diagnostic names the rule and what breaks it, and
// points at the caller's line. Checking sorts the rows by key and by path;
// only the rows outside a catalog are compared in pairs, for a handler they
// share.
template<auto const& Rows, auto const& Areas, auto const& Groups>
  requires some_od_rows<std::remove_reference_t<decltype(Rows)>>
        && some_od_areas<std::remove_reference_t<decltype(Areas)>>
        && some_od_groups<std::remove_reference_t<decltype(Groups)>>
consteval auto make_dictionary()
{
  using traits =
      detail::od_rows_traits<std::remove_reference_t<decltype(Rows)>>;
  using context_type = typename traits::context_type;

  static_assert(detail::od_check(Rows, Areas, Groups).empty(),
                detail::od_check(Rows, Areas, Groups));

  constexpr auto records =
      static_cast<std::size_t>(std::ranges::count_if(Rows, [](auto const& row) {
        return !row.binding.layout.empty();
      }));
  constexpr auto arrays =
      static_cast<std::size_t>(std::ranges::count_if(Rows, [](auto const& row) {
        return row.binding.count != nullptr;
      }));

  constexpr auto plain = traits::size - records - arrays;
  constexpr auto implied = detail::od_implied_count(Rows);

  od_dictionary<context_type, plain + implied, records, arrays> dictionary;
  std::array<od_entry<context_type>, plain> entries{};
  auto entry = entries.begin();
  auto record = dictionary.records_.begin();
  auto array = dictionary.arrays_.begin();
  for (auto const& row : Rows) {
    auto const& binding = row.binding;
    if (!binding.layout.empty()) {
      *record++ = {.index = row.key.index,
                   .arg = binding.arg,
                   .layout = binding.layout};
    }
    else if (binding.count != nullptr) {
      *array++ = {.index = row.key.index,
                  .type = binding.type,
                  .access = binding.access,
                  .count = binding.count,
                  .read = binding.read,
                  .write = binding.write};
    }
    else {
      *entry++ = {.index = row.key.index,
                  .subindex = row.key.subindex,
                  .type = binding.type,
                  .access = binding.access,
                  .arg = binding.arg,
                  .read = binding.read,
                  .write = binding.write};
    }
  }
  std::ranges::sort(entries, {}, &od_entry<context_type>::key);

  // Each index whose first entry is past 00h gets its subindex 00h, before
  // that entry, reading the subindex of its last.
  auto out = dictionary.entries_.begin();
  for (auto i = 0uz; i < plain; ++i) {
    auto const index = entries[i].index;
    bool const first = (i == 0) || (entries[i - 1].index != index);
    if (first && entries[i].subindex != 0) {
      auto last = i;
      while ((last + 1 < plain) && (entries[last + 1].index == index)) {
        ++last;
      }
      *out++ = {.index = index,
                .subindex = 0,
                .type = od_data_type::uint8,
                .access = od_access::const_,
                .arg = entries[last].subindex,
                .read = &detail::od_highest_subindex<context_type>,
                .write = nullptr};
    }
    *out++ = entries[i];
  }

  std::ranges::sort(dictionary.records_, {}, &od_record<context_type>::index);
  std::ranges::sort(dictionary.arrays_, {}, &od_array<context_type>::index);
  dictionary.fingerprint_ = detail::od_fingerprint(Rows, Areas);
  return dictionary;
}

// The class template `od_view` refers to the entries, the records and the
// arrays of an `od_dictionary`, which it does not own, and looks objects up by
// key. It is built only from a dictionary, never from a temporary one, so what
// it refers to is sorted and checked.
template<typename Ctx>
class od_view {
public:
  template<std::size_t Entries, std::size_t Records, std::size_t Arrays>
  constexpr od_view(
      od_dictionary<Ctx, Entries, Records, Arrays> const& dictionary)
      : entries_(dictionary.entries()),
        records_(dictionary.records()),
        arrays_(dictionary.arrays()),
        fingerprint_(dictionary.fingerprint())
  {
  }

  template<std::size_t Entries, std::size_t Records, std::size_t Arrays>
  od_view(od_dictionary<Ctx, Entries, Records, Arrays> const&&) = delete;

  // Returns the object at `key`: the entry of its row, or the one that reads
  // the highest subindex at subindex 00h of an index whose objects start past
  // it; or the one that the field of its record at its subindex makes, with
  // the record's `arg` as its argument; or, of an array, the count at
  // subindex 00h and at any other the element whose position, the subindex
  // less one, is its argument. Fails with `object_not_found` if no row serves
  // the index of `key`, and with `subindex_not_found` if rows serve the index
  // but not the subindex.
  constexpr std::expected<od_entry<Ctx>, sdo_abort_code> find(od_key key) const
  {
    auto const record = std::ranges::lower_bound(records_,
                                                 key.index,
                                                 {},
                                                 &od_record<Ctx>::index);
    if ((record != records_.end()) && (record->index == key.index)) {
      if (std::size_t{key.subindex} >= record->layout.size()) {
        return std::unexpected(sdo_abort_code::subindex_not_found);
      }
      auto const& field = record->layout[key.subindex];
      return od_entry<Ctx>{.index = key.index,
                           .subindex = key.subindex,
                           .type = field.type,
                           .access = field.access,
                           .arg = record->arg,
                           .read = field.read,
                           .write = field.write};
    }

    auto const array =
        std::ranges::lower_bound(arrays_, key.index, {}, &od_array<Ctx>::index);
    if ((array != arrays_.end()) && (array->index == key.index)) {
      if (key.subindex == 0) {
        return od_entry<Ctx>{.index = key.index,
                             .subindex = 0,
                             .type = od_data_type::uint8,
                             .access = od_access::const_,
                             .arg = 0,
                             .read = array->count,
                             .write = nullptr};
      }
      return od_entry<Ctx>{.index = key.index,
                           .subindex = key.subindex,
                           .type = array->type,
                           .access = array->access,
                           .arg = static_cast<std::uint16_t>(key.subindex - 1),
                           .read = array->read,
                           .write = array->write};
    }

    auto const entry =
        std::ranges::lower_bound(entries_, key, {}, &od_entry<Ctx>::key);
    if ((entry != entries_.end()) && (entry->key() == key)) return *entry;
    // Rows serve the index if the entry after `key` or the one before is at it.
    bool const indexed =
        ((entry != entries_.end()) && (entry->index == key.index))
        || ((entry != entries_.begin())
            && (std::prev(entry)->index == key.index));
    return std::unexpected(indexed ? sdo_abort_code::subindex_not_found
                                   : sdo_abort_code::object_not_found);
  }

  constexpr std::span<od_entry<Ctx> const> entries() const
  {
    return entries_;
  }

  constexpr std::span<od_record<Ctx> const> records() const
  {
    return records_;
  }

  constexpr std::span<od_array<Ctx> const> arrays() const
  {
    return arrays_;
  }

  // Returns the fingerprint of the dictionary (`od_dictionary::fingerprint`).
  constexpr std::uint32_t fingerprint() const
  {
    return fingerprint_;
  }

private:
  std::span<od_entry<Ctx> const> entries_;
  std::span<od_record<Ctx> const> records_;
  std::span<od_array<Ctx> const> arrays_;
  std::uint32_t fingerprint_;
};

} // namespace canopen
} // namespace can
} // namespace emb
