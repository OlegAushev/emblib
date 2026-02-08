#include <emb/algorithm.hpp>

#include <cassert>

namespace emb {
namespace detail {
namespace tests{

constexpr bool test_algorithm() {
  assert(median3(1, 2, 3) == 2);
  assert(median3(10, 2, 3) == 3);
  assert(median3(-1.0f, 2.0f, 0.0f) == 0.0f);
  return true;
}

static_assert(test_algorithm());

}
}
}