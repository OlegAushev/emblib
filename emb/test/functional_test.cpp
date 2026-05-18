#include <emb/functional.hpp>

#include <cassert>

namespace {

constexpr int add3(int a, int b, int c) {
  return a + b + c;
}

constexpr int nullary() {
  return 42;
}

constexpr bool test_curry() {
  assert(emb::curry(nullary) == 42);

  assert(emb::curry(add3, 1, 2, 3) == 6);

  [[maybe_unused]] auto f1 = emb::curry(add3, 1);
  assert(f1(2, 3) == 6);
  assert(f1(10, 20) == 31);

  [[maybe_unused]] auto f2 = emb::curry(add3, 1, 2);
  assert(f2(3) == 6);
  assert(f2(100) == 103);

  [[maybe_unused]] auto f1_then_f2 = emb::curry(add3, 1)(2);
  assert(f1_then_f2(3) == 6);

  assert(emb::curry(add3, 1)(2)(3) == 6);
  assert(emb::curry(add3)(1)(2)(3) == 6);
  assert(emb::curry(add3)(1, 2)(3) == 6);
  assert(emb::curry(add3)(1)(2, 3) == 6);

  [[maybe_unused]] auto lambda = [](int x, int y) { return x * y; };
  assert(emb::curry(lambda, 3, 4) == 12);
  assert(emb::curry(lambda)(3)(4) == 12);

  return true;
}

static_assert(test_curry());

} // namespace
