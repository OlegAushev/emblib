#pragma once

#include <emb/concurrent/isr_seqlock.hpp>
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

template<typename... Statuses>
consteval bool all_empty(typelist<Statuses...>) {
  return (std::is_empty_v<Statuses> && ...);
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
      detail::all_empty(StatusList{}),
      "statuses must be stateless tag types"
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

  template<typename Status, Level L>
    requires valid_level<StatusList, Status, L>
  static void set(Status, std::integral_constant<Level, L>) {
    CriticalSection lock;
    flags_[std::to_underlying(L)].update([](flags_type f) {
      f.set(id_of<Status>);
      return f;
    });
  }

  // for statuses confined to a single level it can be deduced
  template<typename Status>
    requires contains<StatusList, Status>
          && (Status::level_min == Status::level_max)
  static void set(Status s) {
    set(s, std::integral_constant<Level, Status::level_min>{});
  }

  template<typename Status, Level L>
    requires valid_level<StatusList, Status, L>
  static void reset(Status, std::integral_constant<Level, L>) {
    CriticalSection lock;
    flags_[std::to_underlying(L)].update([](flags_type f) {
      f.reset(id_of<Status>);
      return f;
    });
  }

  template<typename Status>
    requires contains<StatusList, Status>
  static void reset(Status s) {
    if constexpr (Status::level_min == Status::level_max) {
      reset(s, std::integral_constant<Level, Status::level_min>{});
    } else {
      CriticalSection lock;
      constexpr auto lo = std::to_underlying(Status::level_min);
      constexpr auto hi = std::to_underlying(Status::level_max);
      for (auto l = lo; l <= hi; ++l) {
        flags_[l].update([](flags_type f) {
          f.reset(id_of<Status>);
          return f;
        });
      }
    }
  }

  template<typename Status, Level L>
    requires valid_level<StatusList, Status, L>
  static bool test(Status, std::integral_constant<Level, L>) {
    return flags_[std::to_underlying(L)].load().test(id_of<Status>);
  }

  template<typename Status>
    requires contains<StatusList, Status>
  static bool test(Status s) {
    if constexpr (Status::level_min == Status::level_max) {
      return test(s, std::integral_constant<Level, Status::level_min>{});
    } else {
      constexpr auto lo = std::to_underlying(Status::level_min);
      constexpr auto hi = std::to_underlying(Status::level_max);
      for (auto l = lo; l <= hi; ++l) {
        if (flags_[l].load().test(id_of<Status>)) return true;
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
    for (auto l = 0uz; l < LevelCount; ++l) {
      flags_[l].store({});
    }
  }

private:
  template<typename Status>
  static constexpr id_type id_of = detail::index_of<Status>(StatusList{});

  inline static std::array<isr_seqlock<flags_type>, LevelCount> flags_{};
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
      detail::all_empty(StatusList{}),
      "statuses must be stateless tag types"
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

  template<typename Status>
    requires contains<StatusList, Status>
  bool test(Status, Level lvl) const {
    return flags_[std::to_underlying(lvl)].test(id_of<Status>);
  }

  template<typename Status>
    requires contains<StatusList, Status>
  bool test(Status) const {
    for (auto l = 0uz; l < LevelCount; ++l) {
      if (flags_[l].test(id_of<Status>)) return true;
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
  template<typename Status>
  static constexpr id_type id_of = detail::index_of<Status>(StatusList{});

  std::array<flags_type, LevelCount> flags_{};
};

} // namespace emb::trouble
