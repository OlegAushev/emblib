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

} // namespace tests
} // namespace internal
} // namespace emb

#endif
