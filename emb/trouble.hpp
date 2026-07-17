#pragma once

#include <emb/concurrent/double_buffer.hpp>
#include <emb/meta.hpp>

#include <array>
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
consteval id_type index_of(typelist<Statuses...>) {
  return static_cast<id_type>(type_index_v<S, Statuses...>);
}

template<typename L, typename... Statuses>
consteval bool all_status_like(typelist<Statuses...>) {
  return (status_like<Statuses, L> && ...);
}

template<typename... Statuses>
consteval bool valid_level_ranges(typelist<Statuses...>) {
  return ((Statuses::level_min <= Statuses::level_max) && ...);
}

template<std::size_t LevelCount, typename... Statuses>
consteval bool levels_within(typelist<Statuses...>) {
  return (
      ... && std::cmp_less(std::to_underlying(Statuses::level_max), LevelCount)
  );
}

} // namespace detail

//
// ---- registry ----
//
template<
    typename StatusList,
    typename Level,
    std::size_t LevelCount,
    typename CriticalSection>
class registry {
  static_assert(level_like<Level>, "Level must be a scoped enum");
  static_assert(
      detail::all_status_like<Level>(StatusList{}),
      "every status must provide level_min/level_max of type Level"
  );
  static_assert(
      typelist_unique_v<StatusList>,
      "status list must not contain duplicate statuses"
  );
  static_assert(
      detail::valid_level_ranges(StatusList{}),
      "level_min must be <= level_max"
  );
  static_assert(
      detail::levels_within<LevelCount>(StatusList{}),
      "status level_max must be within LevelCount"
  );
  static_assert(
      StatusList::size <= std::size_t{std::numeric_limits<id_type>::max()} + 1,
      "status count exceeds id_type range"
  );

public:
  static constexpr std::size_t status_count = StatusList::size;
  static constexpr std::size_t level_count = LevelCount;
  using flags_type = std::bitset<status_count>;

  template<typename S, Level Lvl>
    requires valid_level<StatusList, S, Lvl>
  static void set() {
    CriticalSection lock;
    auto& shadow = shadow_[std::to_underlying(Lvl)];
    shadow.set(id_of<S>);
    flags_[std::to_underlying(Lvl)].store(shadow);
  }

  // for statuses confined to a single level it can be deduced
  template<typename S>
    requires contains<StatusList, S> && (S::level_min == S::level_max)
  static void set() {
    set<S, S::level_min>();
  }

  template<typename S, Level Lvl>
    requires valid_level<StatusList, S, Lvl>
  static void reset() {
    CriticalSection lock;
    auto& shadow = shadow_[std::to_underlying(Lvl)];
    shadow.reset(id_of<S>);
    flags_[std::to_underlying(Lvl)].store(shadow);
  }

  template<typename S>
    requires contains<StatusList, S>
  static void reset() {
    if constexpr (S::level_min == S::level_max) {
      reset<S, S::level_min>();
    } else {
      CriticalSection lock;
      constexpr auto lo = std::to_underlying(S::level_min);
      constexpr auto hi = std::to_underlying(S::level_max);
      for (auto l = lo; l <= hi; ++l) {
        shadow_[l].reset(id_of<S>);
        flags_[l].store(shadow_[l]);
      }
    }
  }

  template<typename S, Level Lvl>
    requires valid_level<StatusList, S, Lvl>
  static bool test() {
    return flags_[std::to_underlying(Lvl)].load().test(id_of<S>);
  }

  template<typename S>
    requires contains<StatusList, S>
  static bool test() {
    if constexpr (S::level_min == S::level_max) {
      return test<S, S::level_min>();
    } else {
      constexpr auto lo = std::to_underlying(S::level_min);
      constexpr auto hi = std::to_underlying(S::level_max);
      for (auto l = lo; l <= hi; ++l) {
        if (flags_[l].load().test(id_of<S>)) return true;
      }
      return false;
    }
  }

  static flags_type flags(Level lvl) {
    return flags_[std::to_underlying(lvl)].load();
  }

  static bool any(Level lvl) {
    return flags(lvl).any();
  }

  static bool any() {
    for (auto l = 0uz; l < LevelCount; ++l) {
      if (flags_[l].load().any()) return true;
    }
    return false;
  }

  static void clear() {
    CriticalSection lock;
    shadow_.fill({});
    for (auto l = 0uz; l < LevelCount; ++l) {
      flags_[l].store({});
    }
  }

private:
  template<typename S>
  static constexpr id_type id_of = detail::index_of<S>(StatusList{});

  inline static std::array<flags_type, LevelCount> shadow_{};
  inline static std::array<double_buffer<flags_type>, LevelCount> flags_{};
};

//
// ---- registry_mirror ----
//
template<typename StatusList, typename Level, std::size_t LevelCount>
class registry_mirror {
  static_assert(level_like<Level>, "Level must be a scoped enum");
  static_assert(
      typelist_unique_v<StatusList>,
      "status list must not contain duplicate statuses"
  );
  static_assert(
      StatusList::size <= std::size_t{std::numeric_limits<id_type>::max()} + 1,
      "status count exceeds id_type range"
  );

public:
  static constexpr std::size_t status_count = StatusList::size;
  static constexpr std::size_t level_count = LevelCount;
  using flags_type = std::bitset<status_count>;

  void store(Level lvl, flags_type flags) {
    flags_[std::to_underlying(lvl)] = flags;
  }

  template<typename S>
    requires contains<StatusList, S>
  bool test(Level lvl) const {
    return flags_[std::to_underlying(lvl)].test(id_of<S>);
  }

  template<typename S>
    requires contains<StatusList, S>
  bool test() const {
    for (auto l = 0uz; l < LevelCount; ++l) {
      if (flags_[l].test(id_of<S>)) return true;
    }
    return false;
  }

  flags_type flags(Level lvl) const {
    return flags_[std::to_underlying(lvl)];
  }

  bool any(Level lvl) const {
    return flags(lvl).any();
  }

  bool any() const {
    for (auto l = 0uz; l < LevelCount; ++l) {
      if (flags_[l].any()) return true;
    }
    return false;
  }

  // Highest level with at least one active status.
  std::optional<Level> worst() const {
    for (auto l = LevelCount; l > 0uz; --l) {
      if (flags_[l - 1].any()) return static_cast<Level>(l - 1);
    }
    return std::nullopt;
  }

  void clear() {
    flags_.fill({});
  }

private:
  template<typename S>
  static constexpr id_type id_of = detail::index_of<S>(StatusList{});

  std::array<flags_type, LevelCount> flags_{};
};

} // namespace emb::trouble
