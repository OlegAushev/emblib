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

struct tracked {
  int value = 0;
  int* counter = nullptr;

  constexpr tracked() = default;
  constexpr tracked(int v, int* c) : value(v), counter(c) {
    if (counter) ++*counter;
  }
  constexpr tracked(tracked const& o) : value(o.value), counter(o.counter) {
    if (counter) ++*counter;
  }
  constexpr tracked(tracked&& o) noexcept
      : value(o.value), counter(o.counter) {
    o.counter = nullptr;
  }
  constexpr tracked& operator=(tracked const& o) {
    if (this != &o) {
      if (counter) --*counter;
      value = o.value;
      counter = o.counter;
      if (counter) ++*counter;
    }
    return *this;
  }
  constexpr tracked& operator=(tracked&& o) noexcept {
    if (this != &o) {
      if (counter) --*counter;
      value = o.value;
      counter = o.counter;
      o.counter = nullptr;
    }
    return *this;
  }
  constexpr ~tracked() {
    if (counter) --*counter;
  }
};

static_assert(!std::is_trivially_destructible_v<tracked>);

constexpr bool test_inplace_queue_lifecycle() {
  int alive = 0;
  {
    emb::inplace_queue<tracked, 4> q;
    assert(q.empty() && alive == 0);

    q.emplace(1, &alive);
    q.emplace(2, &alive);
    assert(q.size() == 2 && alive == 2);
    assert(q.front().value == 1);
    assert(q.back().value == 2);

    q.pop();
    assert(q.size() == 1 && alive == 1);
    assert(q.front().value == 2);

    q.clear();
    assert(q.empty() && alive == 0);

    for (int i = 1; i <= 4; ++i) {
      q.emplace(i, &alive);
    }
    assert(q.full() && alive == 4);

    q.pop();
    q.emplace(5, &alive);
    assert(q.size() == 4 && alive == 4);
    assert(q.front().value == 2);
    assert(q.back().value == 5);
  }
  assert(alive == 0);
  return true;
}

constexpr bool test_inplace_queue_copy() {
  int alive = 0;
  emb::inplace_queue<tracked, 4> q;
  q.emplace(1, &alive);
  q.emplace(2, &alive);
  q.emplace(3, &alive);
  assert(alive == 3);

  {
    auto copy = q;
    assert(copy.size() == 3 && alive == 6);
    assert(copy.front().value == 1);
    assert(copy.back().value == 3);
  }
  assert(alive == 3);

  emb::inplace_queue<tracked, 4> other;
  other.emplace(100, &alive);
  assert(alive == 4);

  other = q;
  assert(other.size() == 3 && alive == 6);
  assert(other.front().value == 1);
  assert(other.back().value == 3);

  return true;
}

constexpr bool test_inplace_queue_move_push() {
  int alive = 0;
  emb::inplace_queue<tracked, 2> q;

  tracked t{42, &alive};
  assert(alive == 1);

  q.push(std::move(t));
  assert(alive == 1);
  assert(q.front().value == 42);

  return true;
}

constexpr bool test_inplace_queue_try_pop() {
  int alive = 0;
  emb::inplace_queue<tracked, 2> q;

  auto empty_pop = q.try_pop();
  assert(!empty_pop.has_value());

  q.emplace(7, &alive);
  assert(alive == 1);

  auto popped = q.try_pop();
  assert(popped.has_value());
  assert(popped->value == 7);
  assert(q.empty());
  assert(alive == 1);

  return true;
}

struct no_default {
  int value;
  constexpr no_default(int v) : value(v) {}
};

static_assert(!std::is_default_constructible_v<no_default>);

constexpr bool test_inplace_queue_no_default_ctor() {
  emb::inplace_queue<no_default, 3> q;
  q.emplace(10);
  q.emplace(20);
  q.emplace(30);
  assert(q.full());
  assert(q.front().value == 10);
  assert(q.back().value == 30);

  q.pop();
  assert(q.front().value == 20);

  return true;
}

constexpr bool test_inplace_queue_move() {
  int alive = 0;
  emb::inplace_queue<tracked, 4> src;
  src.emplace(1, &alive);
  src.emplace(2, &alive);
  src.emplace(3, &alive);
  assert(alive == 3);

  auto dst = std::move(src);
  assert(dst.size() == 3 && alive == 3);
  assert(dst.front().value == 1);
  assert(dst.back().value == 3);
  assert(src.empty());

  emb::inplace_queue<tracked, 4> other;
  other.emplace(99, &alive);
  assert(alive == 4);

  other = std::move(dst);
  assert(other.size() == 3 && alive == 3);
  assert(other.front().value == 1);
  assert(other.back().value == 3);
  assert(dst.empty());

  return true;
}

constexpr bool test_inplace_queue_move_wraparound() {
  int alive = 0;
  emb::inplace_queue<tracked, 4> src;
  for (int i = 1; i <= 4; ++i) {
    src.emplace(i, &alive);
  }
  src.pop();
  src.emplace(5, &alive);
  assert(src.front().value == 2);
  assert(src.back().value == 5);
  assert(alive == 4);

  auto dst = std::move(src);
  assert(dst.size() == 4 && alive == 4);
  assert(dst.front().value == 2);
  assert(dst.back().value == 5);
  assert(src.empty());

  return true;
}

static_assert(test_inplace_queue_lifecycle());
static_assert(test_inplace_queue_copy());
static_assert(test_inplace_queue_move_push());
static_assert(test_inplace_queue_try_pop());
static_assert(test_inplace_queue_no_default_ctor());
static_assert(test_inplace_queue_move());
static_assert(test_inplace_queue_move_wraparound());

} // namespace tests
} // namespace internal
} // namespace emb

#endif
