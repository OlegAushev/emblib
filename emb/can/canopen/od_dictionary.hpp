#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <iterator>
#include <span>
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

} // namespace detail

// The concept `some_od_rows<T>` is satisfied if and only if `T` is a C array
// of `od_row<Ctx> const` for some `Ctx`.
template<typename T>
concept some_od_rows =
    requires { typename detail::od_rows_traits<T>::context_type; };

template<typename Ctx, std::size_t Entries, std::size_t Records>
class od_dictionary;

template<auto const& Rows>
  requires some_od_rows<std::remove_reference_t<decltype(Rows)>>
consteval auto make_dictionary();

// The class template `od_dictionary` holds the entries of a dictionary sorted
// by key, and its records sorted by index. Only `make_dictionary` creates a
// dictionary, after checking the rows it is made of, so every dictionary, and
// every `od_view` of one, meets the server's preconditions: keys are unique
// and sorted, a record has its index to itself, and an entry or a field has a
// reader exactly if its access is readable and a writer exactly if it is
// writable.
template<typename Ctx, std::size_t Entries, std::size_t Records>
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

private:
  consteval od_dictionary() = default;

  template<auto const& Rows>
    requires some_od_rows<std::remove_reference_t<decltype(Rows)>>
  friend consteval auto make_dictionary();

  std::array<od_entry<Ctx>, Entries> entries_{};
  std::array<od_record<Ctx>, Records> records_{};
};

// Returns the dictionary made of `Rows`, a C array of `od_row<Ctx> const`
// that must be an `inline constexpr` variable of a named namespace: GCC at
// -O0 emits any other table, the names of its objects included. A row whose
// binding has a layout becomes a record, any other an entry. The program is
// ill-formed if the rows break a rule of `detail::od_check`; the diagnostic
// names the rule and the rows that break it, and points at the caller's line.
// Checking takes time quadratic in the number of rows.
template<auto const& Rows>
  requires some_od_rows<std::remove_reference_t<decltype(Rows)>>
consteval auto make_dictionary()
{
  using traits =
      detail::od_rows_traits<std::remove_reference_t<decltype(Rows)>>;
  using context_type = typename traits::context_type;

  static_assert(detail::od_check(Rows).empty(), detail::od_check(Rows));

  constexpr auto records =
      static_cast<std::size_t>(std::ranges::count_if(Rows, [](auto const& row) {
        return !row.binding.layout.empty();
      }));

  od_dictionary<context_type, traits::size - records, records> dictionary;
  auto entry = dictionary.entries_.begin();
  auto record = dictionary.records_.begin();
  for (auto const& row : Rows) {
    auto const& binding = row.binding;
    if (binding.layout.empty()) {
      *entry++ = {.index = row.key.index,
                  .subindex = row.key.subindex,
                  .type = binding.type,
                  .access = binding.access,
                  .arg = binding.arg,
                  .read = binding.read,
                  .write = binding.write};
    }
    else {
      *record++ = {.index = row.key.index,
                   .arg = binding.arg,
                   .aux = binding.aux,
                   .layout = binding.layout};
    }
  }
  std::ranges::sort(dictionary.entries_, {}, &od_entry<context_type>::key);
  std::ranges::sort(dictionary.records_, {}, &od_record<context_type>::index);
  return dictionary;
}

// The class template `od_view` refers to the entries and the records of an
// `od_dictionary`, which it does not own, and looks objects up by key. It is
// built only from a dictionary, never from a temporary one, so what it refers
// to is sorted and checked.
template<typename Ctx>
class od_view {
public:
  template<std::size_t Entries, std::size_t Records>
  constexpr od_view(od_dictionary<Ctx, Entries, Records> const& dictionary)
      : entries_(dictionary.entries()), records_(dictionary.records())
  {
  }

  template<std::size_t Entries, std::size_t Records>
  od_view(od_dictionary<Ctx, Entries, Records> const&&) = delete;

  // Returns the object at `key`: the entry of its row, or the one that the
  // field of its record at its subindex makes, with the record's `arg` or
  // `aux` as its argument. Fails with `object_not_found` if no row serves the
  // index of `key`, and with `subindex_not_found` if rows serve the index but
  // not the subindex.
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
                           .arg = field.aux ? record->aux : record->arg,
                           .read = field.read,
                           .write = field.write};
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

private:
  std::span<od_entry<Ctx> const> entries_;
  std::span<od_record<Ctx> const> records_;
};

} // namespace canopen
} // namespace can
} // namespace emb
