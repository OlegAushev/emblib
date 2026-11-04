#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

#include <emb/meta/typelist.hpp>
#include <emb/trouble.hpp>

namespace {

using emb::trouble::detail::slot_layout;

// -- Masks --

static_assert(slot_layout<4>::slots_per_word == 8);
static_assert(slot_layout<4>::slot_mask == 0xFu);
static_assert(slot_layout<4>::level_masks[0] == 0x11111111u);
static_assert(slot_layout<4>::level_masks[3] == 0x88888888u);
static_assert(slot_layout<4>::level_masks_from[0] == 0xFFFFFFFFu);
static_assert(slot_layout<4>::level_masks_from[1] == 0xEEEEEEEEu);
static_assert(slot_layout<4>::level_masks_from[3] == 0x88888888u);

// A slot that does not divide the word leaves the top bits unused.
static_assert(slot_layout<3>::slots_per_word == 10);
static_assert(slot_layout<3>::level_masks[0] == 0x09249249u);
static_assert(slot_layout<3>::level_masks_from[0] == 0x3FFFFFFFu);

// -- Positions --

// The ninth status of four levels opens the second word.
static_assert(slot_layout<4>::word_of(7) == 0);
static_assert(slot_layout<4>::word_of(8) == 1);
static_assert(slot_layout<4>::shift_of(9) == 4);
static_assert(slot_layout<4>::flag_of(9, 2) == 0x40u);

// Ten statuses of three levels to a word, the last slot just under the
// unused bits.
static_assert(slot_layout<3>::word_of(10) == 1);
static_assert(slot_layout<3>::shift_of(9) == 27);
static_assert(slot_layout<3>::flag_of(9, 2) == 0x20000000u);

// -- Rules --

// The reference the rules are checked against: the top flag of a slot,
// found one level at a time.
template<std::size_t LevelCount>
consteval std::optional<std::size_t> top_flag(std::uint32_t slot)
{
  std::optional<std::size_t> top;
  for (auto l = 0uz; l < LevelCount; ++l) {
    if (((slot >> l) & 1u) != 0) top = l;
  }
  return top;
}

template<std::size_t LevelCount>
consteval bool test_severity_of()
{
  using layout = slot_layout<LevelCount>;
  for (std::uint32_t slot = 0; slot <= layout::slot_mask; ++slot) {
    if (layout::severity_of(slot) != top_flag<LevelCount>(slot)) {
      return false;
    }
  }
  return true;
}

// Every value of a slot at every position, the other slots all clear or all
// set. top_flags_at only clears flags it found, so a neighbour of all ones
// shows any flag that leaks from one slot into the next.
template<std::size_t LevelCount>
consteval bool test_top_flags_at()
{
  using layout = slot_layout<LevelCount>;
  using word_type = typename layout::word_type;
  for (auto const fill : {word_type{0}, layout::slot_mask}) {
    auto const fill_top = top_flag<LevelCount>(fill);
    for (word_type value = 0; value <= layout::slot_mask; ++value) {
      auto const value_top = top_flag<LevelCount>(value);
      for (auto pos = 0uz; pos < layout::slots_per_word; ++pos) {
        word_type word = 0;
        for (auto s = 0uz; s < layout::slots_per_word; ++s) {
          word |= (s == pos ? value : fill) << (s * layout::slot_bits);
        }
        for (auto l = 0uz; l < LevelCount; ++l) {
          word_type expected = 0;
          for (auto s = 0uz; s < layout::slots_per_word; ++s) {
            auto const top = s == pos ? value_top : fill_top;
            if (top == l) {
              expected |= word_type{1} << (s * layout::slot_bits + l);
            }
          }
          if (layout::top_flags_at(word, l) != expected) return false;
        }
      }
    }
  }
  return true;
}

static_assert(test_severity_of<4>());
static_assert(test_severity_of<3>());
static_assert(test_top_flags_at<4>());
static_assert(test_top_flags_at<3>());

// -- Declarations --

enum class level : std::uint8_t {
  warning,
  error,
  critical,
  emergency
};

struct overheat {
  static constexpr level levels[] = {level::warning, level::critical};
};

struct overvoltage {
  static constexpr level levels[] = {level::emergency};
};

struct link_lost {
  static constexpr level levels[] = {level::warning,
                                     level::error,
                                     level::critical};
};

struct no_levels {};

struct int_levels {
  static constexpr int levels[] = {0};
};

struct level_twice {
  static constexpr level levels[] = {level::warning, level::warning};
};

namespace detail = emb::trouble::detail;
using emb::typelist;

static_assert(detail::all_status_like<level>(
    typelist<overheat, overvoltage, link_lost>{}));
static_assert(!detail::all_status_like<level>(typelist<no_levels>{}));
static_assert(!detail::all_status_like<level>(typelist<int_levels>{}));

static_assert(detail::levels_distinct<level>(typelist<overheat, link_lost>{}));
static_assert(!detail::levels_distinct<level>(typelist<level_twice>{}));

static_assert(detail::levels_within<level, 4>(typelist<overvoltage>{}));
static_assert(!detail::levels_within<level, 3>(typelist<overvoltage>{}));

// A status without levels is all_status_like's to report: the other checks
// pass it rather than fail to compile.
static_assert(detail::levels_within<level, 4>(typelist<no_levels>{}));
static_assert(detail::levels_distinct<level>(typelist<int_levels>{}));

// -- Registry --

using statuses = typelist<overheat, overvoltage, link_lost>;
using registry = emb::trouble::registry<statuses, level, 4>;

template<level L>
using at = std::integral_constant<level, L>;

template<typename R, typename S, typename L>
concept raisable_at = requires(R r, S s, L l) { r.raise(s, l); };

template<typename R, typename S>
concept raisable = requires(R r, S s) { r.raise(s); };

template<typename R, typename S, typename L>
concept trackable_at = requires(R r, S s, L l, bool b) { r.track(s, l, b); };

template<typename R, typename S>
concept trackable = requires(R r, S s, bool b) { r.track(s, b); };

// A level is raised or tracked only where the status declares it.
static_assert(raisable_at<registry, overheat, at<level::critical>>);
static_assert(!raisable_at<registry, overheat, at<level::error>>);
static_assert(trackable_at<registry, link_lost, at<level::error>>);
static_assert(!trackable_at<registry, link_lost, at<level::emergency>>);

// Without a level only for a status of a single level.
static_assert(raisable<registry, overvoltage>);
static_assert(trackable<registry, overvoltage>);
static_assert(!raisable<registry, overheat>);
static_assert(!trackable<registry, overheat>);

// A status of another list is nobody's here.
struct stranger {
  static constexpr level levels[] = {level::warning};
};

static_assert(emb::trouble::status_like<stranger, level>);
static_assert(!raisable<registry, stranger>);
static_assert(!raisable_at<registry, stranger, at<level::warning>>);

// Eight statuses of four levels share a word, and a ninth takes another.
template<std::size_t I>
struct numbered {
  static constexpr level levels[] = {level::warning};
};

using eight = typelist<numbered<0>,
                       numbered<1>,
                       numbered<2>,
                       numbered<3>,
                       numbered<4>,
                       numbered<5>,
                       numbered<6>,
                       numbered<7>>;

static_assert(sizeof(emb::trouble::registry<eight, level, 4>) == 4);
static_assert(
    sizeof(emb::trouble::registry<emb::typelist_append_t<eight, numbered<8>>,
                                  level,
                                  4>)
    == 8);

} // namespace
