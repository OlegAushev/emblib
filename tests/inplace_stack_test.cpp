#include <emb/inplace_stack.hpp>

namespace emb {
namespace internal {
namespace tests {

template<typename S>
constexpr bool test_inplace_stack(S s)
  requires(std::same_as<typename S::value_type, int>) {
  int const cap{static_cast<int>(s.capacity())};

  assert(s.empty());

  s.push(1);
  assert(!s.empty() && s.size() == 1 && s.top() == 1);

  if (s.capacity() != 1) {
    assert(!s.full());
  } else {
    assert(s.full());
  }

  s.pop();
  assert(s.empty() && !s.full());

  for (auto i{1}; i <= cap; ++i) {
    s.push(i);
    assert(s.top() == i);
  }

  assert(s.full());
  assert(s.top() == cap);

  for (auto i{cap}; i >= 1; --i) {
    assert(s.top() == i);
    s.pop();
  }

  assert(s.empty());

  for (auto i{1}; i <= cap; ++i) {
    s.push(i);
  }
  assert(s.full());

  s.clear();
  assert(s.empty() && s.size() == 0);

  return true;
}

static_assert(test_inplace_stack(emb::inplace_stack<int, 1>{}));
static_assert(test_inplace_stack(emb::inplace_stack<int, 2>{}));
static_assert(test_inplace_stack(emb::inplace_stack<int, 5>{}));
static_assert(test_inplace_stack(emb::inplace_stack<int, 10>{}));

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

constexpr bool test_inplace_stack_lifecycle() {
  int alive = 0;
  {
    emb::inplace_stack<tracked, 4> s;
    assert(s.empty() && alive == 0);

    s.emplace(1, &alive);
    s.emplace(2, &alive);
    assert(s.size() == 2 && alive == 2);
    assert(s.top().value == 2);

    s.pop();
    assert(s.size() == 1 && alive == 1);
    assert(s.top().value == 1);

    s.clear();
    assert(s.empty() && alive == 0);

    for (int i = 1; i <= 4; ++i) {
      s.emplace(i, &alive);
    }
    assert(s.full() && alive == 4);
    assert(s.top().value == 4);
  }
  assert(alive == 0);
  return true;
}

constexpr bool test_inplace_stack_copy() {
  int alive = 0;
  emb::inplace_stack<tracked, 4> s;
  s.emplace(1, &alive);
  s.emplace(2, &alive);
  s.emplace(3, &alive);
  assert(alive == 3);

  {
    auto copy = s;
    assert(copy.size() == 3 && alive == 6);
    assert(copy.top().value == 3);
  }
  assert(alive == 3);

  emb::inplace_stack<tracked, 4> other;
  other.emplace(100, &alive);
  assert(alive == 4);

  other = s;
  assert(other.size() == 3 && alive == 6);
  assert(other.top().value == 3);

  return true;
}

constexpr bool test_inplace_stack_move() {
  int alive = 0;
  emb::inplace_stack<tracked, 4> src;
  src.emplace(1, &alive);
  src.emplace(2, &alive);
  src.emplace(3, &alive);
  assert(alive == 3);

  auto dst = std::move(src);
  assert(dst.size() == 3 && alive == 3);
  assert(dst.top().value == 3);
  assert(src.empty());

  emb::inplace_stack<tracked, 4> other;
  other.emplace(99, &alive);
  assert(alive == 4);

  other = std::move(dst);
  assert(other.size() == 3 && alive == 3);
  assert(other.top().value == 3);
  assert(dst.empty());

  return true;
}

constexpr bool test_inplace_stack_move_push() {
  int alive = 0;
  emb::inplace_stack<tracked, 2> s;

  tracked t{42, &alive};
  assert(alive == 1);

  s.push(std::move(t));
  assert(alive == 1);
  assert(s.top().value == 42);

  return true;
}

constexpr bool test_inplace_stack_try_pop() {
  int alive = 0;
  emb::inplace_stack<tracked, 2> s;

  auto empty_pop = s.try_pop();
  assert(!empty_pop.has_value());

  s.emplace(7, &alive);
  assert(alive == 1);

  auto popped = s.try_pop();
  assert(popped.has_value());
  assert(popped->value == 7);
  assert(s.empty());
  assert(alive == 1);

  return true;
}

constexpr bool test_inplace_stack_try_push() {
  emb::inplace_stack<int, 2> s;
  assert(s.try_push(1));
  assert(s.try_push(2));
  assert(!s.try_push(3));
  assert(s.size() == 2 && s.top() == 2);

  return true;
}

struct no_default {
  int value;
  constexpr no_default(int v) : value(v) {}
};

static_assert(!std::is_default_constructible_v<no_default>);

constexpr bool test_inplace_stack_no_default_ctor() {
  emb::inplace_stack<no_default, 3> s;
  s.emplace(10);
  s.emplace(20);
  s.emplace(30);
  assert(s.full());
  assert(s.top().value == 30);

  s.pop();
  assert(s.top().value == 20);

  return true;
}

static_assert(test_inplace_stack_lifecycle());
static_assert(test_inplace_stack_copy());
static_assert(test_inplace_stack_move());
static_assert(test_inplace_stack_move_push());
static_assert(test_inplace_stack_try_pop());
static_assert(test_inplace_stack_try_push());
static_assert(test_inplace_stack_no_default_ctor());

} // namespace tests
} // namespace internal
} // namespace emb
