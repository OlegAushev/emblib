#ifdef __cpp_constexpr

#include <emb/inplace_queue.hpp>

namespace emb {
namespace internal {
namespace tests {

template<typename Q>
constexpr bool test_inplace_queue(Q q)
  requires(std::same_as<typename Q::value_type, int>) {
  int const cap{static_cast<int>(q.capacity())};

  assert(q.empty());

  q.push(1);
  assert(!q.empty() && q.size() == 1 && q.front() == 1 && q.back() == 1);

  if (q.capacity() != 1) {
    assert(!q.full());
  } else {
    assert(q.full());
  }

  q.pop();
  assert(q.empty() && !q.full());

  for (auto i{1}; i <= cap; ++i) {
    q.push(i);
    assert(q.back() == i);
  }

  assert(q.full());
  assert(q.front() == 1);
  assert(q.back() == cap);

  q.pop();
  assert(!q.full());

  q.push(cap + 1);
  assert(q.back() == cap + 1);

  if (q.capacity() != 1) {
    assert(q.front() == 2);
  } else {
    assert(q.front() == cap + 1);
  }

  int val{q.front()};
  while (q.size() > 1) {
    assert(q.front() == val);
    q.pop();
    ++val;
  }

  assert(q.front() == cap + 1);

  q.clear();
  assert(q.empty());

  for (auto i{1}; i <= cap; ++i) {
    q.push(i);
    assert(q.back() == i);
  }

  assert(q.full());
  assert(q.front() == 1);
  assert(q.back() == cap);

  for (auto i{cap + 1}; i <= cap * 2; ++i) {
    q.pop();
    q.push(i);
    assert(q.front() == i - cap + 1);
    assert(q.back() == i);
  }

  assert(q.back() == 2 * cap);
  if (q.capacity() != 1) {
    assert(q.front() == cap + 1);
  } else {
    assert(q.front() == q.back());
  }

  return true;
}

static_assert(test_inplace_queue(emb::inplace_queue<int, 1>{}));
static_assert(test_inplace_queue(emb::inplace_queue<int, 2>{}));
static_assert(test_inplace_queue(emb::inplace_queue<int, 5>{}));
static_assert(test_inplace_queue(emb::inplace_queue<int, 10>{}));

} // namespace tests
} // namespace internal
} // namespace emb

#endif
