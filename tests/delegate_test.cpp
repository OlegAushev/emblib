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
};

constexpr bool test_delegate() {
  // [[maybe_unused]] auto add_delegate =
  //     emb::delegate<int(int, int)>::bind<&add>();
  // assert(add_delegate(1, 2) == 3);

  // [[maybe_unused]] static foo f(-1, 5);
  // [[maybe_unused]] auto foo_delegate =
  //     emb::delegate<int(int)>::bind<&foo::add>(&f);
  // assert(foo_delegate(6) == 10);

  return true;
}

static_assert(test_delegate());

} // namespace tests
} // namespace internal
} // namespace emb
