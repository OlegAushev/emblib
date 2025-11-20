#ifdef __cpp_constexpr

#include <emb/stack.hpp>

namespace emb {
namespace internal {
namespace tests {

constexpr bool test_stack(stack_type auto s)
  requires(std::same_as<typename decltype(s)::value_type, int>) {
  // int const cap{static_cast<int>(s.capacity())};

  assert(s.empty());

  // q.push(1);
  // assert(
  //     !q.empty() && q.size() == 1 && q.front() == 1 && q.back() == 1);

  // if (q.capacity() != 1) {
  //   assert(!q.full());
  // } else {
  //   assert(q.full());
  // }

  // q.pop();
  // assert(q.empty() && !q.full());

  // for (auto i{1}; i <= cap; ++i) {
  //   q.push(i);
  //   assert(q.back() == i);
  // }

  // assert(q.full());
  // assert(q.front() == 1);
  // assert(q.back() == cap);

  // q.pop();
  // assert(!q.full());

  // q.push(cap + 1);
  // assert(q.back() == cap + 1);

  // if (q.capacity() != 1) {
  //   assert(q.front() == 2);
  // } else {
  //   assert(q.front() == cap + 1);
  // }

  // int val{q.front()};
  // while (q.size() > 1) {
  //   assert(q.front() == val);
  //   q.pop();
  //   ++val;
  // }

  // assert(q.front() == cap + 1);

  // q.clear();
  // assert(q.empty());

  // for (auto i{1}; i <= cap; ++i) {
  //   q.push(i);
  //   assert(q.back() == i);
  // }

  // assert(q.full());
  // assert(q.front() == 1);
  // assert(q.back() == cap);

  // for (auto i{cap + 1}; i <= cap * 2; ++i) {
  //   q.pop();
  //   q.push(i);
  //   assert(q.front() == i - cap + 1);
  //   assert(q.back() == i);
  // }

  // assert(q.back() == 2 * cap);
  // if (q.capacity() != 1) {
  //   assert(q.front() == cap + 1);
  // } else {
  //   assert(q.front() == q.back());
  // }

  return true;
}

static_assert(test_stack(emb::stack<int, 1>{}));
static_assert(test_stack(emb::stack<int>{1}));
static_assert(test_stack(emb::v1::stack<int, 1>{}));

static_assert(test_stack(emb::stack<int, 2>{}));
static_assert(test_stack(emb::stack<int>{2}));
static_assert(test_stack(emb::v1::stack<int, 2>{}));

static_assert(test_stack(emb::stack<int, 5>{}));
static_assert(test_stack(emb::stack<int>{5}));
static_assert(test_stack(emb::v1::stack<int, 5>{}));

static_assert(test_stack(emb::stack<int, 10>{}));
static_assert(test_stack(emb::stack<int>{10}));
static_assert(test_stack(emb::v1::stack<int, 10>{}));

} // namespace tests
} // namespace internal
} // namespace emb

#endif

#if 0
#include <emb/tests/tests.hpp>

void emb::tests::stack_test() {
#ifdef EMB_TESTS_ENABLED
  emb::stack<int, 3> stack;

  EMB_ASSERT_TRUE(stack.empty());
  EMB_ASSERT_TRUE(!stack.full());

  stack.push(1);
  EMB_ASSERT_TRUE(!stack.empty());
  EMB_ASSERT_TRUE(!stack.full());
  EMB_ASSERT_EQUAL(stack.size(), 1);
  EMB_ASSERT_EQUAL(stack.top(), 1);

  stack.pop();
  EMB_ASSERT_TRUE(stack.empty());
  EMB_ASSERT_TRUE(!stack.full());

  stack.push(2);
  stack.push(3);
  stack.push(4);
  EMB_ASSERT_TRUE(stack.full());
  EMB_ASSERT_EQUAL(stack.size(), 3);
  EMB_ASSERT_EQUAL(stack.top(), 4);

  if (!stack.full()) {
    stack.push(5);
  }
  EMB_ASSERT_TRUE(stack.full());
  EMB_ASSERT_EQUAL(stack.size(), 3);
  EMB_ASSERT_EQUAL(stack.top(), 4);

  stack.pop();
  EMB_ASSERT_TRUE(!stack.empty());
  EMB_ASSERT_TRUE(!stack.full());
  EMB_ASSERT_EQUAL(stack.size(), 2);
  EMB_ASSERT_EQUAL(stack.top(), 3);

  stack.pop();
  EMB_ASSERT_TRUE(!stack.empty());
  EMB_ASSERT_TRUE(!stack.full());
  EMB_ASSERT_EQUAL(stack.size(), 1);
  EMB_ASSERT_EQUAL(stack.top(), 2);

  stack.pop();
  EMB_ASSERT_TRUE(stack.empty());

  stack.push(2);
  stack.push(3);
  stack.push(4);
  EMB_ASSERT_TRUE(stack.full());

  stack.clear();
  EMB_ASSERT_TRUE(stack.empty());
#endif
}
#endif
