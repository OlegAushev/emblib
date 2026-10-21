#include <emb/meta.hpp>

#include <array>
#include <cassert>
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

// Names the symbol of its unit, found by argument-dependent lookup.
struct volts {
  float value = 0.f;
  friend constexpr std::string_view unit_symbol(volts)
  {
    return "V";
  }
};

// Names a symbol, but not as a std::string_view.
struct c_string_symbol {
  friend constexpr char const* unit_symbol(c_string_symbol)
  {
    return "V";
  }
};

static_assert(unit_with_symbol<volts>);
static_assert(!unit_with_symbol<c_string_symbol>);
static_assert(!unit_with_symbol<meters>);
static_assert(!unit_with_symbol<float>);

static_assert(unit_symbol_of<volts>() == "V");
static_assert(unit_symbol_of<meters>().empty());
static_assert(unit_symbol_of<float>().empty());

} // namespace
