#ifdef __cpp_constexpr

#include <emblib/tests/tests.hpp>

#include <emblib/queue.hpp>

namespace emb {
namespace internal {
namespace tests {

constexpr bool test_queue(Queue auto q)
  requires(std::same_as<typename decltype(q)::value_type, int>) {
  int const cap{static_cast<int>(q.capacity())};

  EMB_CONSTEXPR_ASSERT(q.empty());

  q.push(1);
  EMB_CONSTEXPR_ASSERT(
      !q.empty() && q.size() == 1 && q.front() == 1 && q.back() == 1);

  if (q.capacity() != 1) {
    EMB_CONSTEXPR_ASSERT(!q.full());
  } else {
    EMB_CONSTEXPR_ASSERT(q.full());
  }

  q.pop();
  EMB_CONSTEXPR_ASSERT(q.empty() && !q.full());

  for (auto i{1}; i <= cap; ++i) {
    q.push(i);
    EMB_CONSTEXPR_ASSERT(q.back() == i);
  }

  EMB_CONSTEXPR_ASSERT(q.full());
  EMB_CONSTEXPR_ASSERT(q.front() == 1);
  EMB_CONSTEXPR_ASSERT(q.back() == cap);

  q.pop();
  EMB_CONSTEXPR_ASSERT(!q.full());

  q.push(cap + 1);
  EMB_CONSTEXPR_ASSERT(q.back() == cap + 1);

  if (q.capacity() != 1) {
    EMB_CONSTEXPR_ASSERT(q.front() == 2);
  } else {
    EMB_CONSTEXPR_ASSERT(q.front() == cap + 1);
  }

  int val{q.front()};
  while (q.size() > 1) {
    EMB_CONSTEXPR_ASSERT(q.front() == val);
    q.pop();
    ++val;
  }

  EMB_CONSTEXPR_ASSERT(q.front() == cap + 1);

  q.clear();
  EMB_CONSTEXPR_ASSERT(q.empty());

  for (auto i{1}; i <= cap; ++i) {
    q.push(i);
    EMB_CONSTEXPR_ASSERT(q.back() == i);
  }

  EMB_CONSTEXPR_ASSERT(q.full());
  EMB_CONSTEXPR_ASSERT(q.front() == 1);
  EMB_CONSTEXPR_ASSERT(q.back() == cap);

  for (auto i{cap + 1}; i <= cap * 2; ++i) {
    q.pop();
    q.push(i);
    EMB_CONSTEXPR_ASSERT(q.front() == i - cap + 1);
    EMB_CONSTEXPR_ASSERT(q.back() == i);
  }

  EMB_CONSTEXPR_ASSERT(q.back() == 2 * cap);
  if (q.capacity() != 1) {
    EMB_CONSTEXPR_ASSERT(q.front() == cap + 1);
  } else {
    EMB_CONSTEXPR_ASSERT(q.front() == q.back());
  }

  return true;
}

static_assert(test_queue(emb::queue<int, 1>{}));
static_assert(test_queue(emb::queue<int>{1}));
static_assert(test_queue(emb::v1::queue<int, 1>{}));

static_assert(test_queue(emb::queue<int, 2>{}));
static_assert(test_queue(emb::queue<int>{2}));
static_assert(test_queue(emb::v1::queue<int, 2>{}));

static_assert(test_queue(emb::queue<int, 5>{}));
static_assert(test_queue(emb::queue<int>{5}));
static_assert(test_queue(emb::v1::queue<int, 5>{}));

static_assert(test_queue(emb::queue<int, 10>{}));
static_assert(test_queue(emb::queue<int>{10}));
static_assert(test_queue(emb::v1::queue<int, 10>{}));

} // namespace tests
} // namespace internal
} // namespace emb

#endif
