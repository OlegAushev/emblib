#include <emb/functional.hpp>

#include <cassert>

namespace emb {
namespace internal {
namespace tests {

constexpr int add3(int a, int b, int c) {
  return a + b + c;
}

constexpr int nullary() {
  return 42;
}

constexpr bool test_curry() {
  assert(curry(nullary) == 42);

  assert(curry(add3, 1, 2, 3) == 6);

  auto f1 = curry(add3, 1);
  assert(f1(2, 3) == 6);
  assert(f1(10, 20) == 31);

  auto f2 = curry(add3, 1, 2);
  assert(f2(3) == 6);
  assert(f2(100) == 103);

  auto f1_then_f2 = curry(add3, 1)(2);
  assert(f1_then_f2(3) == 6);

  assert(curry(add3, 1)(2)(3) == 6);
  assert(curry(add3)(1)(2)(3) == 6);
  assert(curry(add3)(1, 2)(3) == 6);
  assert(curry(add3)(1)(2, 3) == 6);

  auto lambda = [](int x, int y) { return x * y; };
  assert(curry(lambda, 3, 4) == 12);
  assert(curry(lambda)(3)(4) == 12);

  return true;
}

static_assert(test_curry());

} // namespace tests
} // namespace internal
} // namespace emb
