#include <emb/delegate.hpp>

#include <cassert>

namespace emb {
namespace internal {
namespace tests {

constexpr int add(int a, int b) {
  return a + b;
}

class foo {
  int a_;
  int b_;
public:
  constexpr foo(int a, int b) : a_(a), b_(b) {}

  constexpr int add(int c) { return a_ + b_ + c; }

  constexpr int sum() const { return a_ + b_; }
};

constexpr bool test_delegate() {
  [[maybe_unused]] auto add_delegate =
      emb::delegate<int(int, int)>::bind<&add>();
  assert(add_delegate(1, 2) == 3);

  [[maybe_unused]] foo f(-1, 5);
  [[maybe_unused]] auto foo_add_delegate =
      emb::delegate<int(int)>::bind<&foo::add>(&f);
  assert(foo_add_delegate(6) == 10);
  [[maybe_unused]] auto foo_sum_delegate =
      emb::delegate<int()>::bind<&foo::sum>(&f);
  assert(foo_sum_delegate() == 4);

  return true;
}

static_assert(test_delegate());

} // namespace tests
} // namespace internal
} // namespace emb
