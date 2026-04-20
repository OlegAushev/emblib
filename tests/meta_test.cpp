#ifdef __cpp_constexpr

#include <emb/meta.hpp>

#include <array>
#include <cassert>

namespace emb {
namespace internal {
namespace tests {

constexpr bool test_unroll() {
  std::array<int, 5> arr;
  emb::unroll<5>([&]<size_t I>() {
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

// unique concept
static_assert(typelist_unique<typelist<A, B, C>>);
static_assert(!typelist_unique<typelist<A, A>>);

// append
static_assert(std::is_same_v<typelist_append_t<typelist<>, A>, typelist<A>>);
static_assert(std::is_same_v<typelist_append_t<typelist<A>, B>, typelist<A, B>>);
static_assert(std::is_same_v<typelist_append_t<typelist<A, B>, C>, typelist<A, B, C>>);

} // namespace tests
} // namespace internal
} // namespace emb

#endif
