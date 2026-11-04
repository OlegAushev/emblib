#pragma once

#include <emb/meta.hpp>

#include <array>
#include <atomic>
#include <bit>
#include <bitset>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace emb::trouble {

template<typename L>
concept level_like = std::is_scoped_enum_v<L>;

// A status names the levels it can stand at, as an array of them:
//   static constexpr level levels[] = {level::warning, level::critical};
// The count is taken with std::size, not from the declared type: in a class
// template the bound of such an array is known only once its definition is
// instantiated, and decltype does not instantiate it.
template<typename S, typename L>
concept status_like = level_like<L> && requires {
  requires std::is_array_v<decltype(S::levels)>;
  requires std::same_as<
      std::remove_cv_t<std::remove_extent_t<decltype(S::levels)>>,
      L>;
  requires(std::size(S::levels) > 0);
};

namespace detail {

// Whether the status `S` declares the level `lvl`.
template<typename S, typename L>
consteval bool declares_level(L lvl)
{
  for (auto const l : S::levels) {
    if (l == lvl) return true;
  }
  return false;
}

template<typename S>
concept declares_single_level =
    requires { requires(std::size(S::levels) == 1); };

} // namespace detail

template<auto Lvl, typename S, typename List>
concept level_for = listed_in<S, List>
                 && status_like<S, decltype(Lvl)>
                 && detail::declares_level<S>(Lvl);

// The id of the status `S` in `StatusList`: its zero-based position there, by
// which a protocol addresses it. Inserting or retiring a status moves the
// ones after it.
template<typename S, typename StatusList>
  requires listed_in<S, StatusList>
consteval std::size_t id_of()
{
  return typelist_index_v<S, StatusList>;
}

namespace detail {

template<typename L, typename... Statuses>
consteval bool all_status_like(typelist<Statuses...>)
{
  return (status_like<Statuses, L> && ...);
}

template<typename... Statuses>
consteval bool all_empty(typelist<Statuses...>)
{
  return (std::is_empty_v<Statuses> && ...);
}

// Whether every level a status declares lies in 0 ... LevelCount - 1. A
// status that declares no levels of type L passes: it is all_status_like's
// to report.
template<typename L, std::size_t LevelCount, typename... Statuses>
consteval bool levels_within(typelist<Statuses...>)
{
  auto within = []<typename S>() {
    if constexpr (status_like<S, L>) {
      for (auto const l : S::levels) {
        auto const value = std::to_underlying(l);
        if (std::cmp_less(value, 0) || !std::cmp_less(value, LevelCount)) {
          return false;
        }
      }
    }
    return true;
  };
  return (within.template operator()<Statuses>() && ...);
}

// Whether no status declares a level twice. A status that declares no levels
// of type L passes, as above.
template<typename L, typename... Statuses>
consteval bool levels_distinct(typelist<Statuses...>)
{
  auto distinct = []<typename S>() {
    if constexpr (status_like<S, L>) {
      constexpr auto count = std::size(S::levels);
      for (auto i = 0uz; i < count; ++i) {
        for (auto j = i + 1; j < count; ++j) {
          if (S::levels[i] == S::levels[j]) return false;
        }
      }
    }
    return true;
  };
  return (distinct.template operator()<Statuses>() && ...);
}

} // namespace detail

// Calls f(Status{}) for every status in the list, in declaration order. The
// vocabulary a status carries beyond its levels — names, texts, whatever the
// product needs — stays in the product: it reads it here.
template<typelist_specialization StatusList, typename F>
constexpr void for_each(F&& f)
{
  [&]<typename... Statuses>(typelist<Statuses...>) {
    (f(Statuses{}), ...);
  }(StatusList{});
}

namespace detail {

// Where the flags of the statuses lie in the words of a registry: a slot of
// LevelCount bits per status, bit l the flag of level l, as many slots to a
// word as fit whole. The masks and the rules here take words as values, so
// they can be checked at compile time, which the registry's atomics cannot.
template<std::size_t LevelCount>
struct slot_layout {
  using word_type = std::uint32_t;

  static constexpr std::size_t word_bits =
      std::size_t{std::numeric_limits<word_type>::digits};

  static_assert(LevelCount > 0 && LevelCount <= word_bits,
                "the levels of a status must fit in one word");

  static constexpr std::size_t slot_bits = LevelCount;
  static constexpr std::size_t slots_per_word = word_bits / slot_bits;
  static constexpr word_type slot_mask =
      ~word_type{0} >> (word_bits - slot_bits);

  // The word that holds the slot of the status of id `id`, and the bit its
  // slot starts at there.
  static constexpr std::size_t word_of(std::size_t id)
  {
    return id / slots_per_word;
  }

  static constexpr std::size_t shift_of(std::size_t id)
  {
    return (id % slots_per_word) * slot_bits;
  }

  // The flag of level lvl of the status of id `id`, in the word that holds
  // its slot.
  static constexpr word_type flag_of(std::size_t id, std::size_t lvl)
  {
    return word_type{1} << (shift_of(id) + lvl);
  }

  // level_masks[l] picks the flag of level l out of every slot in a word
  static constexpr std::array<word_type, LevelCount> level_masks = [] {
    std::array<word_type, LevelCount> p{};
    for (auto l = 0uz; l < LevelCount; ++l) {
      for (auto s = 0uz; s < slots_per_word; ++s) {
        p[l] |= word_type{1} << (s * slot_bits + l);
      }
    }
    return p;
  }();

  // level_masks_from[l] picks the flags of level l and of every level above it
  static constexpr std::array<word_type, LevelCount> level_masks_from = [] {
    std::array<word_type, LevelCount> p{};
    word_type from = 0;
    for (auto l = LevelCount; l > 0uz; --l) {
      from |= level_masks[l - 1];
      p[l - 1] = from;
    }
    return p;
  }();

  // The level of the top flag among the flags of one slot, which is the
  // severity of its status.
  static constexpr std::optional<std::size_t> severity_of(word_type flags)
  {
    if (flags == 0) return std::nullopt;
    return static_cast<std::size_t>(std::bit_width(flags) - 1);
  }

  // The flags of level lvl in a word that are the top flag of their slot, with
  // no flag standing above them: the statuses whose severity is lvl. A flag
  // above is moved down to lvl inside its own slot, so no slot reads its
  // neighbour.
  static constexpr word_type top_flags_at(word_type word, std::size_t lvl)
  {
    auto bits = word & level_masks[lvl];
    for (auto k = lvl + 1; k < LevelCount; ++k) {
      bits &= ~((word & level_masks[k]) >> (k - lvl));
    }
    return bits;
  }
};

} // namespace detail

// Every level a status declares has a flag of its own, and the status stands
// at the highest level whose flag stands: its severity. A call sets or clears
// one flag and leaves the others as they stand, so no call has to know what
// else does.
//
// What takes a level down is the kind of call that puts it up:
//
//   - raise() sets a flag that stays until an acknowledgement, clear();
//   - track() follows a condition its producer evaluates: the flag stands
//     while the condition holds.
//
// How long an event keeps a status up, and the hysteresis of a threshold, are
// the producer's: it keeps the state they need and passes track() what it
// decided.
//
// The flags of a status share one slot of one word, so that
//
//   - every change to a status is a single atomic read-modify-write of one
//     word, callable from any interrupt without masking anything;
//   - "is anything at least this bad" is one masked load per word;
//   - the statuses whose severity is exactly l, which is what a protocol
//     usually puts on the wire, come out of the same masks.
//
// Nothing here locks, so nothing has to be told about interrupt priorities.
// The accesses are relaxed: a flag carries no data of its own, and on a
// single core an interrupt sees the writes of what it preempted.
template<typename StatusList, typename Level, std::size_t LevelCount>
class registry {
  static_assert(level_like<Level>, "Level must be a scoped enum");
  static_assert(LevelCount > 0, "LevelCount must not be zero");
  static_assert(detail::all_status_like<Level>(StatusList{}),
                "every status must declare levels, a non-empty array of "
                "Level");
  static_assert(duplicate_free_typelist<StatusList>,
                "status list must not contain duplicate statuses");
  static_assert(detail::all_empty(StatusList{}),
                "statuses must be stateless tag types");
  static_assert(detail::levels_within<Level, LevelCount>(StatusList{}),
                "status levels must lie between 0 and LevelCount - 1");
  static_assert(detail::levels_distinct<Level>(StatusList{}),
                "a status must not declare a level twice");

  using layout = detail::slot_layout<LevelCount>;

public:
  static constexpr std::size_t status_count = StatusList::size;
  static constexpr std::size_t level_count = LevelCount;
  using flags_type = std::bitset<status_count>;

  // Raises the status at L: sets the flag of L, which stays until an
  // acknowledgement, and leaves the other levels as they stand. One atomic
  // or: safe from any interrupt, and no interrupt is masked.
  template<typename Status, Level L>
    requires level_for<L, Status, StatusList>
  void raise(Status, std::integral_constant<Level, L>)
  {
    constexpr auto id = id_of<Status, StatusList>();
    words_[layout::word_of(id)].fetch_or(layout::flag_of(id, index_of(L)),
                                         std::memory_order::relaxed);
  }

  // for a status of a single level it can be deduced
  template<listed_in<StatusList> Status>
    requires detail::declares_single_level<Status>
  void raise(Status s)
  {
    raise(s, std::integral_constant<Level, Status::levels[0]>{});
  }

  // Follows a condition at L: the flag of L stands while `active` is true and
  // goes down once it is false; the other levels stay as they stand. A
  // tracked level belongs to its producer, which calls this from one context
  // whenever it evaluates the condition: whatever else sets or clears the
  // flag — a raise(), another context, an acknowledgement — lasts until the
  // producer's next call. Most calls change nothing and cost one load: the
  // write a call skips would not have changed the flag, so skipping it is
  // right whoever else writes the word.
  template<typename Status, Level L>
    requires level_for<L, Status, StatusList>
  void track(Status, std::integral_constant<Level, L>, bool active)
  {
    constexpr auto id = id_of<Status, StatusList>();
    constexpr auto flag = layout::flag_of(id, index_of(L));
    auto& word = words_[layout::word_of(id)];
    bool const stands = (word.load(std::memory_order::relaxed) & flag) != 0;
    if (active == stands) return;
    if (active) {
      word.fetch_or(flag, std::memory_order::relaxed);
    }
    else {
      word.fetch_and(~flag, std::memory_order::relaxed);
    }
  }

  // for a status of a single level it can be deduced
  template<listed_in<StatusList> Status>
    requires detail::declares_single_level<Status>
  void track(Status s, bool active)
  {
    track(s, std::integral_constant<Level, Status::levels[0]>{}, active);
  }

  // Takes the status down at every level, raised or tracked: an
  // acknowledgement. A tracked level is back at its producer's next call.
  template<listed_in<StatusList> Status>
  void clear(Status)
  {
    constexpr auto id = id_of<Status, StatusList>();
    words_[layout::word_of(id)].fetch_and(
        ~(layout::slot_mask << layout::shift_of(id)),
        std::memory_order::relaxed);
  }

  void clear()
  {
    for (auto& word : words_) {
      word.store(0, std::memory_order::relaxed);
    }
  }

  template<listed_in<StatusList> Status>
  bool active(Status) const
  {
    return flags_of(id_of<Status, StatusList>()) != 0;
  }

  template<listed_in<StatusList> Status>
  std::optional<Level> severity(Status) const
  {
    auto const lvl = layout::severity_of(flags_of(id_of<Status, StatusList>()));
    if (!lvl) return std::nullopt;
    return static_cast<Level>(*lvl);
  }

  // Anything standing at lvl or above it.
  bool at_least(Level lvl) const
  {
    auto const mask = layout::level_masks_from[index_of(lvl)];
    for (auto const& word : words_) {
      if ((word.load(std::memory_order::relaxed) & mask) != 0) return true;
    }
    return false;
  }

  // Anything whose severity is lvl exactly.
  bool exactly(Level lvl) const
  {
    auto const l = index_of(lvl);
    for (auto const& word : words_) {
      auto const bits = word.load(std::memory_order::relaxed);
      if (layout::top_flags_at(bits, l) != 0) return true;
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
      if ((seen & layout::level_masks[l - 1]) != 0) {
        return static_cast<Level>(l - 1);
      }
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

  // The statuses whose severity is lvl — the view a protocol carries. A
  // status whose flag of lvl stands under a higher one is not among them.
  flags_type flags_at(Level lvl) const
  {
    auto const l = index_of(lvl);
    flags_type flags;
    for (auto w = 0uz; w < word_count; ++w) {
      auto const word = words_[w].load(std::memory_order::relaxed);
      auto const bits = layout::top_flags_at(word, l);
      if (bits == 0) continue;
      for (auto s = 0uz; s < layout::slots_per_word; ++s) {
        auto const id = w * layout::slots_per_word + s;
        if (id >= status_count) break;
        if ((bits & layout::flag_of(id, l)) != 0) flags.set(id);
      }
    }
    return flags;
  }

private:
  using word_type = typename layout::word_type;

  static_assert(std::atomic<word_type>::is_always_lock_free,
                "the registry's word must be lock-free");

  static constexpr std::size_t word_count =
      (status_count + layout::slots_per_word - 1) / layout::slots_per_word;

  static constexpr std::size_t index_of(Level lvl)
  {
    return static_cast<std::size_t>(std::to_underlying(lvl));
  }

  word_type flags_of(std::size_t id) const
  {
    auto const word =
        words_[layout::word_of(id)].load(std::memory_order::relaxed);
    return (word >> layout::shift_of(id)) & layout::slot_mask;
  }

  std::array<std::atomic<word_type>, word_count> words_{};
};

// App-side forwarding function objects. Instantiate them as inline constexpr
// objects in the namespace holding the statuses, naming the registry they
// speak for:
//   inline constinit registry_type registry{};
//   inline constexpr emb::trouble::raise_fn<registry> raise{};
//
// The parameter is the object, not its type: the call sites keep saying what
// happened, and which registry remembers it is settled once, here.
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
struct track_fn {
  template<typename Status, typename LevelTag>
    requires requires(Status s, LevelTag l, bool b) { Registry.track(s, l, b); }
  static void operator()(Status s, LevelTag l, bool active)
  {
    Registry.track(s, l, active);
  }

  template<typename Status>
    requires requires(Status s, bool b) { Registry.track(s, b); }
  static void operator()(Status s, bool active)
  {
    Registry.track(s, active);
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

// The read side of a protocol that carries one level's flags per frame: it
// keeps what the wire carries — the statuses whose severity is each level —
// and answers in the same vocabulary as the registry.
template<typename StatusList, typename Level, std::size_t LevelCount>
class registry_mirror {
  static_assert(level_like<Level>, "Level must be a scoped enum");
  static_assert(LevelCount > 0, "LevelCount must not be zero");
  static_assert(detail::all_status_like<Level>(StatusList{}),
                "every status must declare levels, a non-empty array of "
                "Level");
  static_assert(duplicate_free_typelist<StatusList>,
                "status list must not contain duplicate statuses");
  static_assert(detail::all_empty(StatusList{}),
                "statuses must be stateless tag types");
  static_assert(detail::levels_within<Level, LevelCount>(StatusList{}),
                "status levels must lie between 0 and LevelCount - 1");
  static_assert(detail::levels_distinct<Level>(StatusList{}),
                "a status must not declare a level twice");

public:
  static constexpr std::size_t status_count = StatusList::size;
  static constexpr std::size_t level_count = LevelCount;
  using flags_type = std::bitset<status_count>;

  void store(Level lvl, flags_type flags)
  {
    flags_[index_of(lvl)] = flags;
  }

  template<listed_in<StatusList> Status>
  bool active(Status) const
  {
    for (auto const& flags : flags_) {
      if (flags.test(id_of<Status, StatusList>())) return true;
    }
    return false;
  }

  template<listed_in<StatusList> Status>
  std::optional<Level> severity(Status) const
  {
    for (auto l = LevelCount; l > 0uz; --l) {
      if (flags_[l - 1].test(id_of<Status, StatusList>())) {
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
