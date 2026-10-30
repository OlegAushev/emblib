#include <emb/meta.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <string_view>

namespace {

using namespace emb;

constexpr bool test_unroll()
{
  std::array<int, 5> arr;
  emb::unroll<5>([&]<std::size_t I>() {
    arr[I] = I;
    ++arr[I];
  });
  assert(arr[0] == 1);
  assert(arr[1] == 2);
  assert(arr[2] == 3);
  assert(arr[3] == 4);
  assert(arr[4] == 5);
  return true;
}

static_assert(test_unroll());

// -- typelist tests --

struct A {};
struct B {};
struct C {};

// -- nth_type tests --

static_assert(std::is_same_v<nth_type_t<0, A, B, C>, A>);
static_assert(std::is_same_v<nth_type_t<1, A, B, C>, B>);
static_assert(std::is_same_v<nth_type_t<2, A, B, C>, C>);
static_assert(std::is_same_v<nth_type<1, A, B, C>::type, B>);

// -- type_index tests --

static_assert(type_index_v<A, A, B, C> == 0);
static_assert(type_index_v<C, A, B, C> == 2);
static_assert(type_index_v<B, A, B, B> == 1);
static_assert(type_index<B, A, B>::value == 1);

// -- alternative_of tests --

static_assert(is_alternative_of_v<A, std::variant<A, B>>);
static_assert(!is_alternative_of_v<C, std::variant<A, B>>);
static_assert(!is_alternative_of_v<A, int>);
static_assert(alternative_of<B, std::variant<A, B>>);
static_assert(!alternative_of<C, std::variant<A, B>>);

// size
static_assert(typelist<>::size == 0);
static_assert(typelist<A>::size == 1);
static_assert(typelist<A, B, C>::size == 3);
static_assert(typelist_size_v<typelist<>> == 0);
static_assert(typelist_size_v<typelist<A>> == 1);
static_assert(typelist_size_v<typelist<A, B, C>> == 3);

// contains
static_assert(typelist_contains_v<typelist<A, B, C>, A>);
static_assert(typelist_contains_v<typelist<A, B, C>, B>);
static_assert(typelist_contains_v<typelist<A, B, C>, C>);
static_assert(!typelist_contains_v<typelist<A, B>, C>);
static_assert(!typelist_contains_v<typelist<>, A>);
static_assert(!typelist_contains_v<A, A>);

// contains concept
static_assert(typelist_contains<typelist<A, B>, A>);
static_assert(!typelist_contains<typelist<A, B>, C>);

// at
static_assert(std::is_same_v<typelist_at_t<typelist<A, B, C>, 0>, A>);
static_assert(std::is_same_v<typelist_at_t<typelist<A, B, C>, 1>, B>);
static_assert(std::is_same_v<typelist_at_t<typelist<A, B, C>, 2>, C>);

// count
static_assert(typelist_count_v<typelist<A, B, A, C, A>, A> == 3);
static_assert(typelist_count_v<typelist<A, B, A, C, A>, B> == 1);
static_assert(typelist_count_v<typelist<A, B, A, C, A>, C> == 1);
static_assert(typelist_count_v<typelist<A, B>, C> == 0);
static_assert(typelist_count_v<typelist<>, A> == 0);

// unique
static_assert(typelist_unique_v<typelist<A, B, C>>);
static_assert(typelist_unique_v<typelist<A>>);
static_assert(typelist_unique_v<typelist<>>);
static_assert(!typelist_unique_v<typelist<A, B, A>>);
static_assert(!typelist_unique_v<typelist<A, A>>);
static_assert(!typelist_unique_v<A>);

// unique concept
static_assert(typelist_unique<typelist<A, B, C>>);
static_assert(!typelist_unique<typelist<A, A>>);
static_assert(!typelist_unique<A>);

// append
static_assert(std::is_same_v<typelist_append_t<typelist<>, A>, typelist<A>>);
static_assert(
    std::is_same_v<typelist_append_t<typelist<A>, B>, typelist<A, B>>);
static_assert(
    std::is_same_v<typelist_append_t<typelist<A, B>, C>, typelist<A, B, C>>);

// concat
static_assert(
    std::is_same_v<typelist_concat_t<typelist<>, typelist<>>, typelist<>>);
static_assert(
    std::is_same_v<typelist_concat_t<typelist<A>, typelist<>>, typelist<A>>);
static_assert(
    std::is_same_v<typelist_concat_t<typelist<>, typelist<B>>, typelist<B>>);
static_assert(std::is_same_v<typelist_concat_t<typelist<A, B>, typelist<C, A>>,
                             typelist<A, B, C, A>>);

// -- wrapper tests --

// Wraps a float and lets value_of find it by argument-dependent lookup.
struct meters {
  using value_type = float;
  float value;
  constexpr explicit meters(float v) : value(v) {}
  friend constexpr float value_of(meters m)
  {
    return m.value;
  }
};

// Names a value type but cannot be unwrapped: it has no value_of.
struct no_value_of {
  using value_type = int;
  int value;
};

// Unwraps to a reference rather than to a value.
struct by_reference {
  using value_type = int;
  int value;
  constexpr explicit by_reference(int v) : value(v) {}
  friend constexpr int const& value_of(by_reference const& r)
  {
    return r.value;
  }
};

static_assert(some_wrapper<meters>);
static_assert(!some_wrapper<no_value_of>);
static_assert(!some_wrapper<by_reference>);
static_assert(!some_wrapper<float>);

static_assert(std::is_same_v<unwrapped_t<meters>, float>);
static_assert(std::is_same_v<unwrapped_t<float>, float>);
static_assert(std::is_same_v<unwrapped_t<no_value_of>, no_value_of>);

// -- unit symbol tests --

// Names the symbol of its unit in each language, found by argument-dependent
// lookup.
struct volts {
  float value = 0.f;
  friend constexpr i18n::text unit_symbol(volts)
  {
    return {"V", "В"};
  }
};

// Names a symbol in one language, as a std::string_view, not as a text.
struct one_language_symbol {
  friend constexpr std::string_view unit_symbol(one_language_symbol)
  {
    return "V";
  }
};

// Names a symbol, but not as a text.
struct c_string_symbol {
  friend constexpr char const* unit_symbol(c_string_symbol)
  {
    return "V";
  }
};

static_assert(unit_with_symbol<volts>);
static_assert(!unit_with_symbol<one_language_symbol>);
static_assert(!unit_with_symbol<c_string_symbol>);
static_assert(!unit_with_symbol<meters>);
static_assert(!unit_with_symbol<float>);

static_assert(unit_symbol_of<volts>() == i18n::text{"V", "В"});
static_assert(unit_symbol_of<one_language_symbol>() == i18n::text{});
static_assert(unit_symbol_of<meters>() == i18n::text{});
static_assert(unit_symbol_of<float>() == i18n::text{});

// -- enumerator info tests --

// Labels three of its values, the last one apart from the others, and leaves
// one unlabelled.
enum class mode : std::uint8_t { off, slow, fast = 4, unlabelled = 7 };

consteval std::span<enumerator_info<mode> const> enum_info(mode)
{
  static constexpr enumerator_info<mode> info[] = {
      {mode::off, "off", {"Off", "Выкл."}},
      {mode::slow, "slow", {"Slow", "Медленно"}},
      {mode::fast, "fast", {"Fast", "Быстро"}},
  };
  return info;
}

// Labels a negative value: its underlying type is signed.
enum class level : std::int16_t { low = -2, high = 3 };

consteval std::span<enumerator_info<level> const> enum_info(level)
{
  static constexpr enumerator_info<level> info[] = {
      {level::low, "low", {"Low", "Низкий"}},
      {level::high, "high", {"High", "Высокий"}},
  };
  return info;
}

// A scoped enumeration that does not label its values.
enum class bare { a };

// Returns its table as a span of `enumerator_info` that is not const.
enum class mutable_table { a };

consteval std::span<enumerator_info<mutable_table>> enum_info(mutable_table)
{
  return {};
}

// An unscoped enumeration.
enum plain { p };

static_assert(enum_with_info<mode>);
static_assert(enum_with_info<level>);
static_assert(!enum_with_info<bare>);
static_assert(!enum_with_info<mutable_table>);
static_assert(!enum_with_info<plain>);
static_assert(!enum_with_info<int>);

static_assert(enum_info_of<mode>().size() == 3);
static_assert(enum_info_of<mode>()[0].value() == mode::off);
static_assert(enum_info_of<mode>()[2].value() == mode::fast);
static_assert(enum_info_of<mode>()[1].name() == "slow");
static_assert(enum_info_of<mode>()[1].label()
              == i18n::text{"Slow", "Медленно"});
static_assert(enum_info_of<level>().front().value() == level::low);
static_assert(enum_info_of<level>().back().value() == level::high);

// Returns the name of `m`, or an empty view if `mode` does not label it: an
// immediate function reads the info of an enumerator.
consteval std::string_view name_of(mode m)
{
  for (auto const& enumerator : enum_info_of<mode>()) {
    if (enumerator.value() == m) return enumerator.name();
  }
  return {};
}

static_assert(name_of(mode::fast) == "fast");
static_assert(name_of(mode::unlabelled).empty());

// Returns the English label of `e`: reading the info of an enumerator makes
// this function template an immediate function, which a constant expression
// may call.
template<typename E>
constexpr std::string_view english_label_of(E e)
{
  for (auto const& enumerator : enum_info_of<E>()) {
    if (enumerator.value() == e) return enumerator.label().en;
  }
  return {};
}

static_assert(english_label_of(level::high) == "High");
static_assert(english_label_of(mode::off) == "Off");

// -- enum carrier tests --

enum class by_u8 : std::uint8_t { a };
enum class by_i8 : std::int8_t { a };
enum class by_i16 : std::int16_t { a };
enum class by_int { a };
enum class by_u32 : std::uint32_t { a };
enum class by_u64 : std::uint64_t { a };
enum class by_bool : bool { no, yes };
enum class by_char : char { a };

template<typename E>
concept carried = requires { typename enum_carrier32_t<E>; };

static_assert(std::same_as<enum_carrier32_t<by_u8>, std::uint32_t>);
static_assert(std::same_as<enum_carrier32_t<by_i8>, std::int32_t>);
static_assert(std::same_as<enum_carrier32_t<by_i16>, std::int32_t>);
static_assert(std::same_as<enum_carrier32_t<by_int>, std::int32_t>);
static_assert(std::same_as<enum_carrier32_t<by_u32>, std::uint32_t>);
static_assert(!carried<by_u64>);
static_assert(!carried<by_bool>);
static_assert(!carried<by_char>);
static_assert(!carried<int>);

} // namespace
