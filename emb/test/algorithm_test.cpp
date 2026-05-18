#include <emb/algorithm.hpp>

#include <cassert>

namespace {

constexpr bool test_algorithm() {
  assert(emb::median3(1, 2, 3) == 2);
  assert(emb::median3(10, 2, 3) == 3);
  assert(emb::median3(-1.0f, 2.0f, 0.0f) == 0.0f);
  return true;
}

static_assert(test_algorithm());

} // namespace
