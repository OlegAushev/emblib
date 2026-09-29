#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
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

template<typename Ctx, std::size_t N>
class od_dictionary;

template<auto const& Rows>
  requires some_od_rows<std::remove_reference_t<decltype(Rows)>>
consteval auto make_dictionary();

// The class template `od_dictionary` holds the entries of a dictionary sorted
// by key, and the one handler that restores the default of the objects that
// have one. Only `make_dictionary` creates a dictionary, after checking the
// rows it is made of, so every dictionary, and every `od_view` of one, meets
// the server's preconditions: keys are unique and sorted, and an entry has a
// reader exactly if its access is readable and a writer exactly if it is
// writable.
template<typename Ctx, std::size_t N>
class od_dictionary {
public:
  constexpr std::span<od_entry<Ctx> const> entries() const
  {
    return entries_;
  }

  constexpr od_restore_fn<Ctx> restore() const
  {
    return restore_;
  }

private:
  consteval od_dictionary() = default;

  template<auto const& Rows>
    requires some_od_rows<std::remove_reference_t<decltype(Rows)>>
  friend consteval auto make_dictionary();

  std::array<od_entry<Ctx>, N> entries_{};
  od_restore_fn<Ctx> restore_ = nullptr;
};

// Returns the dictionary made of `Rows`, a C array of `od_row<Ctx> const`
// that must be an `inline constexpr` variable of a named namespace: GCC at
// -O0 emits any other table, the names of its objects included. The program
// is ill-formed if the rows break a rule of `detail::od_check`; the
// diagnostic names the rule and the rows that break it, and points at the
// caller's line. Checking takes time quadratic in the number of rows.
template<auto const& Rows>
  requires some_od_rows<std::remove_reference_t<decltype(Rows)>>
consteval auto make_dictionary()
{
  using traits =
      detail::od_rows_traits<std::remove_reference_t<decltype(Rows)>>;
  using context_type = typename traits::context_type;

  static_assert(detail::od_check(Rows).empty(), detail::od_check(Rows));

  od_dictionary<context_type, traits::size> dictionary;
  for (auto i = 0uz; i < traits::size; ++i) {
    auto const& row = Rows[i];
    auto const& binding = row.binding;
    dictionary.entries_[i] = {.index = row.key.index,
                              .subindex = row.key.subindex,
                              .type = binding.type,
                              .access = binding.access,
                              .restorable = binding.restore != nullptr,
                              .arg = binding.arg,
                              .read = binding.read,
                              .write = binding.write};
    if (binding.restore != nullptr) {
      dictionary.restore_ = binding.restore;
    }
  }
  std::ranges::sort(dictionary.entries_, {}, &od_entry<context_type>::key);
  return dictionary;
}

// The class template `od_view` refers to the entries of an `od_dictionary`,
// which it does not own, and looks objects up by key. It is built only from a
// dictionary, never from a temporary one, so what it refers to is sorted and
// checked.
template<typename Ctx>
class od_view {
public:
  template<std::size_t N>
  constexpr od_view(od_dictionary<Ctx, N> const& dictionary)
      : entries_(dictionary.entries()), restore_(dictionary.restore())
  {
  }

  template<std::size_t N>
  od_view(od_dictionary<Ctx, N> const&&) = delete;

  // Returns the entry of the object at `key`, or nullptr if there is none.
  constexpr od_entry<Ctx> const* find(od_key key) const
  {
    auto const it =
        std::ranges::lower_bound(entries_, key, {}, &od_entry<Ctx>::key);
    if ((it == entries_.end()) || (it->key() != key)) return nullptr;
    return &*it;
  }

  constexpr od_restore_fn<Ctx> restore() const
  {
    return restore_;
  }

  constexpr std::span<od_entry<Ctx> const> entries() const
  {
    return entries_;
  }

private:
  std::span<od_entry<Ctx> const> entries_;
  od_restore_fn<Ctx> restore_;
};

} // namespace canopen
} // namespace can
} // namespace emb
