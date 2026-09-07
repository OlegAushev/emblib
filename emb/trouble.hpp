#pragma once

#include <emb/meta.hpp>

#include <array>
#include <atomic>
#include <bit>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace emb::trouble {

using id_type = std::uint8_t;

template<typename L>
concept level_like = std::is_scoped_enum_v<L>;

template<typename S, typename L>
concept status_like = level_like<L> && requires {
  { S::level_min } -> std::same_as<L const&>;
  { S::level_max } -> std::same_as<L const&>;
};

template<typename List, typename S>
concept contains = typelist_contains<List, S>;

template<typename List, typename S, auto Lvl>
concept valid_level = contains<List, S>
                   && status_like<S, decltype(Lvl)>
                   && (S::level_min <= Lvl)
                   && (Lvl <= S::level_max);

namespace detail {

template<typename S, typename... Statuses>
consteval id_type index_of(typelist<Statuses...>)
{
  return static_cast<id_type>(type_index_v<S, Statuses...>);
}

template<typename L, typename... Statuses>
consteval bool all_status_like(typelist<Statuses...>)
{
  return (status_like<Statuses, L> && ...);
}

template<typename... Statuses>
consteval bool valid_level_ranges(typelist<Statuses...>)
{
  return ((Statuses::level_min <= Statuses::level_max) && ...);
}

template<typename... Statuses>
consteval bool all_empty(typelist<Statuses...>)
{
  return (std::is_empty_v<Statuses> && ...);
}

template<std::size_t LevelCount, typename... Statuses>
consteval bool levels_within(typelist<Statuses...>)
{
  return (
      ...
      && std::cmp_less(std::to_underlying(Statuses::level_max), LevelCount));
}

} // namespace detail

//
// ---- registry ----
//
// A status holds one severity at a time: the highest level asserted since it
// was last cleared. Every level of a status lives in one slot of one word,
// thermometer-coded — bit k of the slot reads "severity is at least k" — so
// that
//
//   - each update to a status is a single atomic read-modify-write. Nothing
//     can interleave inside an escalation, so a status can never end up
//     visible at a high level while missing from the low ones;
//   - escalating is a plain fetch_or of the prefix: monotone, lock-free,
//     callable from any interrupt without masking anything;
//   - "is anything at least this bad" is one masked load per word;
//   - the statuses sitting exactly at level l, which is what a protocol
//     usually puts on the wire, come out of the same masks.
//
// Nothing here locks, so nothing has to be told about interrupt priorities.
// The accesses are relaxed: a flag carries no data of its own, and on a
// single core an interrupt sees the writes of what it preempted.
//
template<typename StatusList, typename Level, std::size_t LevelCount>
class registry {
  static_assert(level_like<Level>, "Level must be a scoped enum");
  static_assert(LevelCount > 0, "LevelCount must not be zero");
  static_assert(detail::all_status_like<Level>(StatusList{}),
                "every status must provide level_min/level_max of type Level");
  static_assert(typelist_unique_v<StatusList>,
                "status list must not contain duplicate statuses");
  static_assert(detail::valid_level_ranges(StatusList{}),
                "level_min must be <= level_max");
  static_assert(detail::all_empty(StatusList{}),
                "statuses must be stateless tag types");
  static_assert(detail::levels_within<LevelCount>(StatusList{}),
                "status level_max must be within LevelCount");
  static_assert(StatusList::size
                    <= std::size_t{std::numeric_limits<id_type>::max()} + 1,
                "status count exceeds id_type range");

public:
  static constexpr std::size_t status_count = StatusList::size;
  static constexpr std::size_t level_count = LevelCount;
  using flags_type = std::bitset<status_count>;

  //
  // ---- asserting ----
  //

  // Raises the status to L, or leaves it where it stands if that is higher.
  // One atomic or: safe from any interrupt, and no interrupt is masked.
  template<typename Status, Level L>
    requires valid_level<StatusList, Status, L>
  static void set(Status, std::integral_constant<Level, L>)
  {
    escalate(id_of<Status>, index_of(L));
  }

  // for statuses confined to a single level it can be deduced
  template<typename Status>
    requires contains<StatusList, Status>
          && (Status::level_min == Status::level_max)
  static void set(Status s)
  {
    set(s, std::integral_constant<Level, Status::level_min>{});
  }

  // Clears the status if its severity is at most L; one that has escalated
  // above L stands, and only reset(Status) or clear() takes it down. That is
  // what a producer watching a condition at its own level wants: cooling
  // below the warning threshold must not retract the trip above it.
  template<typename Status, Level L>
    requires valid_level<StatusList, Status, L>
  static void reset(Status, std::integral_constant<Level, L>)
  {
    deassert(id_of<Status>, index_of(L));
  }

  template<typename Status>
    requires contains<StatusList, Status>
  static void reset(Status)
  {
    words_[word_of(id_of<Status>)].fetch_and(
        ~(slot_mask << shift_of(id_of<Status>)),
        std::memory_order::relaxed);
  }

  static void clear()
  {
    for (auto& word : words_) {
      word.store(0, std::memory_order::relaxed);
    }
  }

  //
  // ---- querying ----
  //

  template<typename Status>
    requires contains<StatusList, Status>
  static bool active(Status)
  {
    return slot_of(id_of<Status>) != 0;
  }

  template<typename Status>
    requires contains<StatusList, Status>
  static std::optional<Level> severity(Status)
  {
    auto const slot = slot_of(id_of<Status>);
    if (slot == 0) return std::nullopt;
    return static_cast<Level>(std::bit_width(slot) - 1);
  }

  // Anything active at lvl or above it.
  static bool at_least(Level lvl)
  {
    auto const plane = planes[index_of(lvl)];
    for (auto const& word : words_) {
      if ((word.load(std::memory_order::relaxed) & plane) != 0) return true;
    }
    return false;
  }

  // Anything whose severity is lvl exactly.
  static bool exactly(Level lvl)
  {
    auto const l = index_of(lvl);
    for (auto const& word : words_) {
      if (exactly_bits(word.load(std::memory_order::relaxed), l) != 0) {
        return true;
      }
    }
    return false;
  }

  // Highest level with at least one active status.
  static std::optional<Level> worst()
  {
    word_type seen = 0;
    for (auto const& word : words_) {
      seen |= word.load(std::memory_order::relaxed);
    }
    for (auto l = LevelCount; l > 0uz; --l) {
      if ((seen & planes[l - 1]) != 0) return static_cast<Level>(l - 1);
    }
    return std::nullopt;
  }

  // The statuses sitting exactly at lvl — the view a protocol carries.
  static flags_type flags_at(Level lvl)
  {
    auto const l = index_of(lvl);
    flags_type flags;
    for (auto w = 0uz; w < word_count; ++w) {
      auto const bits =
          exactly_bits(words_[w].load(std::memory_order::relaxed), l);
      if (bits == 0) continue;
      for (auto s = 0uz; s < slots_per_word; ++s) {
        auto const id = w * slots_per_word + s;
        if (id >= status_count) break;
        if (((bits >> (s * slot_bits + l)) & 1u) != 0) flags.set(id);
      }
    }
    return flags;
  }

private:
  using word_type = std::uint32_t;
  static constexpr std::size_t word_bits =
      std::size_t{std::numeric_limits<word_type>::digits};

  static_assert(LevelCount <= word_bits,
                "a status's levels must fit in one word");

  // one bit per level, all of a status's levels in one slot
  static constexpr std::size_t slot_bits = LevelCount;
  static constexpr std::size_t slots_per_word = word_bits / slot_bits;
  static constexpr std::size_t word_count =
      (status_count + slots_per_word - 1) / slots_per_word;

  static constexpr word_type slot_mask =
      ~word_type{0} >> (word_bits - slot_bits);

  // planes[l] picks bit l out of every slot in a word
  static constexpr std::array<word_type, LevelCount> planes = [] {
    std::array<word_type, LevelCount> p{};
    for (auto l = 0uz; l < LevelCount; ++l) {
      for (auto s = 0uz; s < slots_per_word; ++s) {
        p[l] |= word_type{1} << (s * slot_bits + l);
      }
    }
    return p;
  }();

  template<typename Status>
  static constexpr id_type id_of = detail::index_of<Status>(StatusList{});

  static constexpr std::size_t index_of(Level lvl)
  {
    return static_cast<std::size_t>(std::to_underlying(lvl));
  }

  static constexpr std::size_t word_of(id_type id)
  {
    return std::size_t{id} / slots_per_word;
  }

  static constexpr std::size_t shift_of(id_type id)
  {
    return (std::size_t{id} % slots_per_word) * slot_bits;
  }

  // the thermometer for severity l: bits 0..l of a slot
  static constexpr word_type prefix_of(std::size_t lvl)
  {
    return slot_mask >> (LevelCount - 1 - lvl);
  }

  static word_type slot_of(id_type id)
  {
    return (words_[word_of(id)].load(std::memory_order::relaxed)
            >> shift_of(id))
         & slot_mask;
  }

  static word_type exactly_bits(word_type word, std::size_t lvl)
  {
    auto bits = word & planes[lvl];
    if (lvl + 1 < LevelCount) {
      bits &= ~((word & planes[lvl + 1]) >> 1);
    }
    return bits;
  }

  static void escalate(id_type id, std::size_t lvl)
  {
    words_[word_of(id)].fetch_or(prefix_of(lvl) << shift_of(id),
                                 std::memory_order::relaxed);
  }

  static void deassert(id_type id, std::size_t lvl)
  {
    auto& word = words_[word_of(id)];
    auto const shift = shift_of(id);
    auto const above =
        (lvl + 1 < LevelCount) ? word_type{1} << (lvl + 1) : word_type{0};

    auto current = word.load(std::memory_order::relaxed);
    while (true) {
      auto const slot = (current >> shift) & slot_mask;
      // already down, or standing above the level being retracted
      if (slot == 0 || (slot & above) != 0) return;
      auto const wanted = current & ~(slot_mask << shift);
      if (word.compare_exchange_weak(current,
                                     wanted,
                                     std::memory_order::relaxed)) {
        return;
      }
    }
  }

  inline static std::array<std::atomic<word_type>, word_count> words_{};
};

//
// ---- app-side forwarding function objects ----
//
// Instantiate as inline constexpr objects in the namespace holding the
// statuses:
//   inline constexpr emb::trouble::set_fn<registry> set{};
//
template<typename Registry>
struct set_fn {
  template<typename Status, typename LevelTag>
    requires requires(Status s, LevelTag l) { Registry::set(s, l); }
  static void operator()(Status s, LevelTag l)
  {
    Registry::set(s, l);
  }

  template<typename Status>
    requires requires(Status s) { Registry::set(s); }
  static void operator()(Status s)
  {
    Registry::set(s);
  }
};

template<typename Registry>
struct reset_fn {
  template<typename Status, typename LevelTag>
    requires requires(Status s, LevelTag l) { Registry::reset(s, l); }
  static void operator()(Status s, LevelTag l)
  {
    Registry::reset(s, l);
  }

  template<typename Status>
    requires requires(Status s) { Registry::reset(s); }
  static void operator()(Status s)
  {
    Registry::reset(s);
  }
};

template<typename Registry>
struct active_fn {
  template<typename Status>
    requires requires(Status s) { Registry::active(s); }
  static bool operator()(Status s)
  {
    return Registry::active(s);
  }
};

template<typename Registry>
struct severity_fn {
  template<typename Status>
    requires requires(Status s) { Registry::severity(s); }
  static auto operator()(Status s)
  {
    return Registry::severity(s);
  }
};

template<typename Registry>
struct at_least_fn {
  static bool operator()(auto lvl)
  {
    return Registry::at_least(lvl);
  }
};

template<typename Registry>
struct exactly_fn {
  static bool operator()(auto lvl)
  {
    return Registry::exactly(lvl);
  }
};

template<typename Registry>
struct worst_fn {
  static auto operator()()
  {
    return Registry::worst();
  }
};

template<typename Registry>
struct flags_at_fn {
  static Registry::flags_type operator()(auto lvl)
  {
    return Registry::flags_at(lvl);
  }
};

template<typename Registry>
struct clear_fn {
  static void operator()()
  {
    Registry::clear();
  }
};

//
// ---- registry_mirror ----
//
// The read side of a protocol that carries one level's flags per frame: it
// keeps what the wire carries — the statuses sitting exactly at each level —
// and answers in the same vocabulary as the registry.
//
template<typename StatusList, typename Level, std::size_t LevelCount>
class registry_mirror {
  static_assert(level_like<Level>, "Level must be a scoped enum");
  static_assert(LevelCount > 0, "LevelCount must not be zero");
  static_assert(typelist_unique_v<StatusList>,
                "status list must not contain duplicate statuses");
  static_assert(detail::all_empty(StatusList{}),
                "statuses must be stateless tag types");
  static_assert(StatusList::size
                    <= std::size_t{std::numeric_limits<id_type>::max()} + 1,
                "status count exceeds id_type range");

public:
  static constexpr std::size_t status_count = StatusList::size;
  static constexpr std::size_t level_count = LevelCount;
  using flags_type = std::bitset<status_count>;

  void store(Level lvl, flags_type flags)
  {
    flags_[index_of(lvl)] = flags;
  }

  template<typename Status>
    requires contains<StatusList, Status>
  bool active(Status) const
  {
    for (auto const& flags : flags_) {
      if (flags.test(id_of<Status>)) return true;
    }
    return false;
  }

  template<typename Status>
    requires contains<StatusList, Status>
  std::optional<Level> severity(Status) const
  {
    for (auto l = LevelCount; l > 0uz; --l) {
      if (flags_[l - 1].test(id_of<Status>)) {
        return static_cast<Level>(l - 1);
      }
    }
    return std::nullopt;
  }

  bool at_least(Level lvl) const
  {
    for (auto l = index_of(lvl); l < LevelCount; ++l) {
      if (flags_[l].any()) return true;
    }
    return false;
  }

  bool exactly(Level lvl) const
  {
    return flags_[index_of(lvl)].any();
  }

  // Highest level with at least one active status.
  std::optional<Level> worst() const
  {
    for (auto l = LevelCount; l > 0uz; --l) {
      if (flags_[l - 1].any()) return static_cast<Level>(l - 1);
    }
    return std::nullopt;
  }

  flags_type flags_at(Level lvl) const
  {
    return flags_[index_of(lvl)];
  }

  void clear()
  {
    flags_.fill({});
  }

private:
  template<typename Status>
  static constexpr id_type id_of = detail::index_of<Status>(StatusList{});

  static constexpr std::size_t index_of(Level lvl)
  {
    return static_cast<std::size_t>(std::to_underlying(lvl));
  }

  std::array<flags_type, LevelCount> flags_{};
};

} // namespace emb::trouble
