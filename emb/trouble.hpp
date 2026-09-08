#pragma once

#include <emb/meta.hpp>

#include <array>
#include <atomic>
#include <bit>
#include <bitset>
#include <concepts>
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
  { S::id } -> std::same_as<id_type const&>;
  { S::level_min } -> std::same_as<L const&>;
  { S::level_max } -> std::same_as<L const&>;
};

// A status may name a group it belongs to: raising it raises the group too,
// so the aggregate cannot be forgotten at a call site.
template<typename S>
concept grouped = requires { typename S::group; };

// What may take a status down. The policy is a property of the status, not
// of the code that raises it: it decides which calls a call site is allowed
// to make at all.
enum class hold_policy : std::uint8_t {
  latched,  // an operator's clear, and nothing else
  tracking, // update() may retract it at the level it watches
  expiring, // refresh() retracts it unless it has been raised again
};

inline constexpr hold_policy latched = hold_policy::latched;
inline constexpr hold_policy tracking = hold_policy::tracking;
inline constexpr hold_policy expiring = hold_policy::expiring;

template<typename S>
concept declares_hold = requires {
  { S::hold } -> std::same_as<hold_policy const&>;
};

// Latched unless the status says otherwise: the policy that keeps whatever
// was raised is the safe default.
template<typename S>
consteval hold_policy hold_of()
{
  if constexpr (declares_hold<S>) {
    return S::hold;
  }
  else {
    return hold_policy::latched;
  }
}

template<typename S, typename L>
concept declares_expiry_level = requires {
  { S::expires_at } -> std::same_as<L const&>;
};

// The level a sweep governs: what recurs at it is retracted, what stands
// above it is somebody else's trip and stays. It defaults to level_min, the
// level a recurring condition normally uses, but a status whose condition
// recurs higher must say so — otherwise the sweep would find a level it does
// not govern and leave the status standing for good.
template<typename S, typename L>
consteval L expires_at_of()
{
  if constexpr (declares_expiry_level<S, L>) {
    return S::expires_at;
  }
  else {
    return S::level_min;
  }
}

// The two conditions a producer evaluates from its own thresholds: one to
// raise on, one to clear on. Between them nothing changes, which is what
// hysteresis is — and the memory it needs is the severity the registry
// already holds, so the producer keeps no state of its own.
struct condition {
  bool raise = false;
  bool clear = false;
};

template<typename List, typename S>
concept contains = typelist_contains<List, S>;

template<typename List, typename S, auto Lvl>
concept valid_level = contains<List, S>
                   && status_like<S, decltype(Lvl)>
                   && (S::level_min <= Lvl)
                   && (Lvl <= S::level_max);

namespace detail {

template<typename... Statuses>
consteval bool ids_unique(typelist<Statuses...>)
{
  std::array<id_type, sizeof...(Statuses)> const ids{Statuses::id...};
  for (auto i = 0uz; i < ids.size(); ++i) {
    for (auto j = i + 1; j < ids.size(); ++j) {
      if (ids[i] == ids[j]) return false;
    }
  }
  return true;
}

template<typename... Statuses>
consteval std::size_t id_space(typelist<Statuses...>)
{
  if constexpr (sizeof...(Statuses) == 0) {
    return 0;
  }
  else {
    std::size_t top = 0;
    ((top = std::size_t{Statuses::id} > top ? std::size_t{Statuses::id} : top),
     ...);
    return top + 1;
  }
}

template<typename... Statuses>
consteval bool any_expiring(typelist<Statuses...>)
{
  return ((hold_of<Statuses>() == hold_policy::expiring) || ...);
}

template<typename... Statuses>
consteval bool groups_valid(typelist<Statuses...>)
{
  using List = typelist<Statuses...>;
  auto valid = []<typename S>() {
    if constexpr (grouped<S>) {
      using G = typename S::group;
      using L = std::remove_cv_t<decltype(S::level_min)>;
      if constexpr (!status_like<G, L>) {
        return false;
      }
      else {
        // in the list, not grouped itself, and wide enough to carry every
        // level the status can be raised to
        return typelist_contains_v<List, G>
            && !grouped<G>
            && (G::level_min <= S::level_min)
            && (S::level_max <= G::level_max);
      }
    }
    else {
      return true;
    }
  };
  return (valid.template operator()<Statuses>() && ...);
}

template<typename L, typename... Statuses>
consteval bool expiry_levels_valid(typelist<Statuses...>)
{
  auto valid = []<typename S>() {
    if constexpr (hold_of<S>() == hold_policy::expiring) {
      auto const lvl = expires_at_of<S, L>();
      return (S::level_min <= lvl) && (lvl <= S::level_max);
    }
    else {
      // naming a level a sweep will never read is a copy-paste, not a wish
      return !declares_expiry_level<S, L>;
    }
  };
  return (valid.template operator()<Statuses>() && ...);
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
  auto within = []<typename S>() {
    return std::cmp_greater_equal(std::to_underlying(S::level_min), 0)
        && std::cmp_less(std::to_underlying(S::level_max), LevelCount);
  };
  return (within.template operator()<Statuses>() && ...);
}

} // namespace detail

// One past the largest id in the list. Storage and the flag masks are sized
// by it, so retiring a status leaves a hole instead of renumbering the ones
// after it — a protocol carrying the flags stays put.
template<some_typelist StatusList>
inline constexpr std::size_t id_space_v = detail::id_space(StatusList{});

// Calls f(Status{}) for every status in the list, in declaration order. The
// vocabulary a status carries beyond its id and levels — names, texts,
// whatever the product needs — stays in the product: it reads it here.
template<some_typelist StatusList, typename F>
constexpr void for_each(F&& f)
{
  [&]<typename... Statuses>(typelist<Statuses...>) {
    (f(Statuses{}), ...);
  }(StatusList{});
}

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
                "every status must provide an id and level_min/level_max of "
                "type Level");
  static_assert(typelist_unique_v<StatusList>,
                "status list must not contain duplicate statuses");
  static_assert(detail::valid_level_ranges(StatusList{}),
                "level_min must be <= level_max");
  static_assert(detail::all_empty(StatusList{}),
                "statuses must be stateless tag types");
  static_assert(detail::levels_within<LevelCount>(StatusList{}),
                "status levels must lie between 0 and LevelCount - 1");
  static_assert(detail::ids_unique(StatusList{}), "status ids must be unique");
  static_assert(detail::groups_valid(StatusList{}),
                "a status's group must be a status in the list, must not be "
                "grouped itself, and must span the status's levels");
  static_assert(detail::expiry_levels_valid<Level>(StatusList{}),
                "expires_at must lie within the status's levels, and only an "
                "expiring status may name one");

public:
  static constexpr std::size_t status_count = StatusList::size;
  static constexpr std::size_t id_space = id_space_v<StatusList>;
  static constexpr std::size_t level_count = LevelCount;
  using flags_type = std::bitset<id_space>;

  //
  // ---- asserting ----
  //

  // Raises the status to L, or leaves it where it stands if that is higher.
  // One atomic or: safe from any interrupt, and no interrupt is masked. A
  // status that names a group raises the group with it — the aggregate is a
  // consequence of the declaration, not of remembering a second call. An
  // expiring status is marked as seen in the same write, so a sweep running
  // concurrently cannot mistake it for stale.
  template<typename Status, Level L>
    requires valid_level<StatusList, Status, L>
  void raise(Status, std::integral_constant<Level, L>)
  {
    escalate<Status>(index_of(L));
    if constexpr (grouped<Status>) {
      escalate<typename Status::group>(index_of(L));
    }
  }

  // for statuses confined to a single level it can be deduced
  template<typename Status>
    requires contains<StatusList, Status>
          && (Status::level_min == Status::level_max)
  void raise(Status s)
  {
    raise(s, std::integral_constant<Level, Status::level_min>{});
  }

  // Follows a condition at L: raises on one edge, retracts on the other,
  // holds between them. Retracting at L leaves a status that stands above it
  // — cooling below the warning threshold must not take back the trip above
  // it — so a status can track at one level and latch at another.
  template<typename Status, Level L>
    requires valid_level<StatusList, Status, L>
          && (hold_of<Status>() == hold_policy::tracking)
  void update(Status s, std::integral_constant<Level, L> lvl, condition cond)
  {
    if (cond.raise) {
      raise(s, lvl);
    }
    else if (cond.clear) {
      deassert(Status::id, index_of(L));
    }
  }

  // no hysteresis: the condition is the whole story
  template<typename Status, Level L>
    requires valid_level<StatusList, Status, L>
          && (hold_of<Status>() == hold_policy::tracking)
  void update(Status s, std::integral_constant<Level, L> lvl, bool active)
  {
    update(s, lvl, condition{.raise = active, .clear = !active});
  }

  // Takes the status down whatever it holds and whatever its policy: an
  // acknowledgement, not a retraction.
  template<typename Status>
    requires contains<StatusList, Status>
  void clear(Status)
  {
    words_[word_of(Status::id)].fetch_and(~(slot_mask << shift_of(Status::id)),
                                          std::memory_order::relaxed);
  }

  void clear()
  {
    for (auto& word : words_) {
      word.store(0, std::memory_order::relaxed);
    }
  }

  // Retracts every expiring status that has not been raised since the last
  // call. Run it from one periodic task: a status then outlives its condition
  // by between one and two periods, and the period is the only thing anyone
  // has to know to read that. A sweep retracts at the status's expires_at,
  // level_min unless it says otherwise, and leaves anything standing above
  // it: the same rule update() follows, so a condition that recurs at its own
  // level cannot take back a trip somebody else raised higher.
  void refresh()
  {
    if constexpr (has_expiring) {
      [this]<typename... Statuses>(typelist<Statuses...>) {
        (
            [this] {
              if constexpr (hold_of<Statuses>() == hold_policy::expiring) {
                sweep(Statuses::id, index_of(expires_at_of<Statuses, Level>()));
              }
            }(),
            ...);
      }(StatusList{});
    }
  }

  //
  // ---- querying ----
  //

  template<typename Status>
    requires contains<StatusList, Status>
  bool active(Status) const
  {
    return (slot_of(Status::id) & level_mask) != 0;
  }

  template<typename Status>
    requires contains<StatusList, Status>
  std::optional<Level> severity(Status) const
  {
    auto const slot = slot_of(Status::id) & level_mask;
    if (slot == 0) return std::nullopt;
    return static_cast<Level>(std::bit_width(slot) - 1);
  }

  // Anything active at lvl or above it.
  bool at_least(Level lvl) const
  {
    auto const plane = planes[index_of(lvl)];
    for (auto const& word : words_) {
      if ((word.load(std::memory_order::relaxed) & plane) != 0) return true;
    }
    return false;
  }

  // Anything whose severity is lvl exactly.
  bool exactly(Level lvl) const
  {
    auto const l = index_of(lvl);
    for (auto const& word : words_) {
      auto const bits = word.load(std::memory_order::relaxed);
      if (exactly_bits(bits, l) != 0) return true;
    }
    return false;
  }

  // Highest level with at least one active status.
  std::optional<Level> worst() const
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

  // Calls f(Status{}, severity) for every status that stands, in
  // declaration order.
  template<typename F>
  void for_each_active(F&& f) const
  {
    [this, &f]<typename... Statuses>(typelist<Statuses...>) {
      (
          [this, &f] {
            if (auto const lvl = severity(Statuses{})) f(Statuses{}, *lvl);
          }(),
          ...);
    }(StatusList{});
  }

  // The statuses sitting exactly at lvl — the view a protocol carries.
  flags_type flags_at(Level lvl) const
  {
    auto const l = index_of(lvl);
    flags_type flags;
    for (auto w = 0uz; w < word_count; ++w) {
      auto const word = words_[w].load(std::memory_order::relaxed);
      auto const bits = exactly_bits(word, l);
      if (bits == 0) continue;
      for (auto s = 0uz; s < slots_per_word; ++s) {
        auto const id = w * slots_per_word + s;
        if (id >= id_space) break;
        if (((bits >> (s * slot_bits + l)) & 1u) != 0) flags.set(id);
      }
    }
    return flags;
  }

private:
  using word_type = std::uint32_t;

  static_assert(std::atomic<word_type>::is_always_lock_free,
                "the registry's word must be lock-free");

  static constexpr std::size_t word_bits =
      std::size_t{std::numeric_limits<word_type>::digits};

  // one bit per level, all of a status's levels in one slot; a list with
  // expiring statuses takes one bit more, the mark a sweep reads and clears.
  // It shares the slot rather than sitting in a word of its own so that
  // raising stays a single write: a sweep can then neither see half a raise
  // nor undo one it did not see.
  static constexpr bool has_expiring = detail::any_expiring(StatusList{});
  static constexpr std::size_t slot_bits = LevelCount + (has_expiring ? 1 : 0);

  static_assert(slot_bits <= word_bits,
                "a status's levels must fit in one word");

  static constexpr std::size_t slots_per_word = word_bits / slot_bits;
  static constexpr std::size_t word_count =
      (id_space + slots_per_word - 1) / slots_per_word;

  static constexpr word_type slot_mask =
      ~word_type{0} >> (word_bits - slot_bits);
  static constexpr word_type level_mask =
      ~word_type{0} >> (word_bits - LevelCount);
  static constexpr word_type seen_bit =
      has_expiring ? word_type{1} << LevelCount : word_type{0};

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
    return level_mask >> (LevelCount - 1 - lvl);
  }

  static constexpr word_type exactly_bits(word_type word, std::size_t lvl)
  {
    auto bits = word & planes[lvl];
    if (lvl + 1 < LevelCount) {
      bits &= ~((word & planes[lvl + 1]) >> 1);
    }
    return bits;
  }

  word_type slot_of(id_type id) const
  {
    auto const word = words_[word_of(id)].load(std::memory_order::relaxed);
    return (word >> shift_of(id)) & slot_mask;
  }

  template<typename Status>
  void escalate(std::size_t lvl)
  {
    constexpr word_type mark =
        hold_of<Status>() == hold_policy::expiring ? seen_bit : word_type{0};
    words_[word_of(Status::id)].fetch_or((prefix_of(lvl) | mark)
                                             << shift_of(Status::id),
                                         std::memory_order::relaxed);
  }

  void deassert(id_type id, std::size_t lvl)
  {
    auto const shift = shift_of(id);
    auto const above =
        lvl + 1 < LevelCount ? word_type{1} << (lvl + 1) : word_type{0};

    auto& word = words_[word_of(id)];
    auto current = word.load(std::memory_order::relaxed);
    while (true) {
      auto const slot = (current >> shift) & level_mask;
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

  // One sweep of an expiring status: the mark set by the last raise buys it
  // this round and is spent doing so; a status that arrives here unmarked has
  // not been raised since the previous sweep and goes down, unless it stands
  // above lvl, which is somebody else's trip and not this sweep's business.
  // Every outcome is one compare-exchange on the word the status lives in, so
  // a raise landing mid-sweep either wins the exchange or is seen by it.
  void sweep(id_type id, std::size_t lvl)
  {
    auto const shift = shift_of(id);
    auto const above =
        lvl + 1 < LevelCount ? word_type{1} << (lvl + 1) : word_type{0};

    auto& word = words_[word_of(id)];
    auto current = word.load(std::memory_order::relaxed);
    while (true) {
      auto const slot = (current >> shift) & slot_mask;
      if ((slot & level_mask) == 0) return;

      word_type wanted;
      if ((slot & seen_bit) != 0) {
        wanted = current & ~(seen_bit << shift);
      }
      else if ((slot & above) != 0) {
        return;
      }
      else {
        wanted = current & ~(slot_mask << shift);
      }

      if (word.compare_exchange_weak(current,
                                     wanted,
                                     std::memory_order::relaxed)) {
        return;
      }
    }
  }

  std::array<std::atomic<word_type>, word_count> words_{};
};

//
// ---- app-side forwarding function objects ----
//
// Instantiate as inline constexpr objects in the namespace holding the
// statuses, naming the registry they speak for:
//   inline constinit registry_type registry{};
//   inline constexpr emb::trouble::raise_fn<registry> raise{};
//
// The parameter is the object, not its type: the call sites keep saying what
// happened, and which registry remembers it is settled once, here.
//
template<auto& Registry>
struct raise_fn {
  template<typename Status, typename LevelTag>
    requires requires(Status s, LevelTag l) { Registry.raise(s, l); }
  static void operator()(Status s, LevelTag l)
  {
    Registry.raise(s, l);
  }

  template<typename Status>
    requires requires(Status s) { Registry.raise(s); }
  static void operator()(Status s)
  {
    Registry.raise(s);
  }
};

template<auto& Registry>
struct update_fn {
  template<typename Status, typename LevelTag>
    requires requires(Status s, LevelTag l, condition c) {
      Registry.update(s, l, c);
    }
  static void operator()(Status s, LevelTag l, condition cond)
  {
    Registry.update(s, l, cond);
  }

  template<typename Status, typename LevelTag>
    requires requires(Status s, LevelTag l, bool b) {
      Registry.update(s, l, b);
    }
  static void operator()(Status s, LevelTag l, bool active)
  {
    Registry.update(s, l, active);
  }
};

template<auto& Registry>
struct refresh_fn {
  static void operator()()
  {
    Registry.refresh();
  }
};

template<auto& Registry>
struct active_fn {
  template<typename Status>
    requires requires(Status s) { Registry.active(s); }
  static bool operator()(Status s)
  {
    return Registry.active(s);
  }
};

template<auto& Registry>
struct severity_fn {
  template<typename Status>
    requires requires(Status s) { Registry.severity(s); }
  static auto operator()(Status s)
  {
    return Registry.severity(s);
  }
};

template<auto& Registry>
struct at_least_fn {
  static bool operator()(auto lvl)
  {
    return Registry.at_least(lvl);
  }
};

template<auto& Registry>
struct exactly_fn {
  static bool operator()(auto lvl)
  {
    return Registry.exactly(lvl);
  }
};

template<auto& Registry>
struct worst_fn {
  static auto operator()()
  {
    return Registry.worst();
  }
};

template<auto& Registry>
struct flags_at_fn {
  static auto operator()(auto lvl)
  {
    return Registry.flags_at(lvl);
  }
};

template<auto& Registry>
struct for_each_active_fn {
  template<typename F>
  static void operator()(F&& f)
  {
    Registry.for_each_active(std::forward<F>(f));
  }
};

template<auto& Registry>
struct clear_fn {
  static void operator()()
  {
    Registry.clear();
  }

  template<typename Status>
    requires requires(Status s) { Registry.clear(s); }
  static void operator()(Status s)
  {
    Registry.clear(s);
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
  static_assert(detail::all_status_like<Level>(StatusList{}),
                "every status must provide an id and level_min/level_max of "
                "type Level");
  static_assert(typelist_unique_v<StatusList>,
                "status list must not contain duplicate statuses");
  static_assert(detail::all_empty(StatusList{}),
                "statuses must be stateless tag types");
  static_assert(detail::levels_within<LevelCount>(StatusList{}),
                "status levels must lie between 0 and LevelCount - 1");
  static_assert(detail::ids_unique(StatusList{}), "status ids must be unique");

public:
  static constexpr std::size_t status_count = StatusList::size;
  static constexpr std::size_t id_space = id_space_v<StatusList>;
  static constexpr std::size_t level_count = LevelCount;
  using flags_type = std::bitset<id_space>;

  void store(Level lvl, flags_type flags)
  {
    flags_[index_of(lvl)] = flags;
  }

  template<typename Status>
    requires contains<StatusList, Status>
  bool active(Status) const
  {
    for (auto const& flags : flags_) {
      if (flags.test(Status::id)) return true;
    }
    return false;
  }

  template<typename Status>
    requires contains<StatusList, Status>
  std::optional<Level> severity(Status) const
  {
    for (auto l = LevelCount; l > 0uz; --l) {
      if (flags_[l - 1].test(Status::id)) {
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

  // Calls f(Status{}, severity) for every status that stands, in
  // declaration order.
  template<typename F>
  void for_each_active(F&& f) const
  {
    [&]<typename... Statuses>(typelist<Statuses...>) {
      (
          [&] {
            if (auto const lvl = severity(Statuses{})) f(Statuses{}, *lvl);
          }(),
          ...);
    }(StatusList{});
  }

  void clear()
  {
    flags_.fill({});
  }

private:
  static constexpr std::size_t index_of(Level lvl)
  {
    return static_cast<std::size_t>(std::to_underlying(lvl));
  }

  std::array<flags_type, LevelCount> flags_{};
};

} // namespace emb::trouble
