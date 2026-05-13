#include <emb/inplace_vector.hpp>

namespace emb {
namespace internal {
namespace tests {

template<typename V>
constexpr bool test_inplace_vector(V v)
  requires(std::same_as<typename V::value_type, int>) {
  int const cap{static_cast<int>(v.capacity())};

  assert(v.empty() && v.size() == 0);

  v.push_back(1);
  assert(!v.empty() && v.size() == 1);
  assert(v.front() == 1 && v.back() == 1 && v[0] == 1);

  if (v.capacity() != 1) {
    assert(!v.full());
  } else {
    assert(v.full());
  }

  v.pop_back();
  assert(v.empty() && !v.full());

  for (auto i{1}; i <= cap; ++i) {
    v.push_back(i);
    assert(v.back() == i);
    assert(v.size() == static_cast<typename V::size_type>(i));
  }

  assert(v.full());
  assert(v.front() == 1 && v.back() == cap);

  int sum{0};
  for (auto i{0}; i < cap; ++i) {
    auto const& x = v[static_cast<typename V::size_type>(i)];
    assert(x == i + 1);
    sum += x;
  }
  assert(sum == cap * (cap + 1) / 2);

  for (auto i{cap}; i >= 1; --i) {
    assert(v.back() == i);
    v.pop_back();
  }
  assert(v.empty());

  for (auto i{1}; i <= cap; ++i) {
    v.push_back(i);
  }
  assert(v.full());

  v.clear();
  assert(v.empty() && v.size() == 0);

  return true;
}

static_assert(test_inplace_vector(emb::inplace_vector<int, 1>{}));
static_assert(test_inplace_vector(emb::inplace_vector<int, 2>{}));
static_assert(test_inplace_vector(emb::inplace_vector<int, 5>{}));
static_assert(test_inplace_vector(emb::inplace_vector<int, 10>{}));

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

constexpr bool test_inplace_vector_lifecycle() {
  int alive = 0;
  {
    emb::inplace_vector<tracked, 4> v;
    assert(v.empty() && alive == 0);

    v.emplace_back(1, &alive);
    v.emplace_back(2, &alive);
    assert(v.size() == 2 && alive == 2);
    assert(v.front().value == 1 && v.back().value == 2);

    v.pop_back();
    assert(v.size() == 1 && alive == 1);
    assert(v.back().value == 1);

    v.clear();
    assert(v.empty() && alive == 0);

    for (int i = 1; i <= 4; ++i) {
      v.emplace_back(i, &alive);
    }
    assert(v.full() && alive == 4);
    assert(v.front().value == 1 && v.back().value == 4);
    assert(v[2].value == 3);
  }
  assert(alive == 0);
  return true;
}

constexpr bool test_inplace_vector_copy() {
  int alive = 0;
  emb::inplace_vector<tracked, 4> v;
  v.emplace_back(1, &alive);
  v.emplace_back(2, &alive);
  v.emplace_back(3, &alive);
  assert(alive == 3);

  {
    auto copy = v;
    assert(copy.size() == 3 && alive == 6);
    assert(copy.front().value == 1 && copy.back().value == 3);
  }
  assert(alive == 3);

  emb::inplace_vector<tracked, 4> other;
  other.emplace_back(100, &alive);
  assert(alive == 4);

  other = v;
  assert(other.size() == 3 && alive == 6);
  assert(other.front().value == 1 && other.back().value == 3);

  return true;
}

constexpr bool test_inplace_vector_move() {
  int alive = 0;
  emb::inplace_vector<tracked, 4> src;
  src.emplace_back(1, &alive);
  src.emplace_back(2, &alive);
  src.emplace_back(3, &alive);
  assert(alive == 3);

  auto dst = std::move(src);
  assert(dst.size() == 3 && alive == 3);
  assert(dst.front().value == 1 && dst.back().value == 3);
  assert(src.empty());

  emb::inplace_vector<tracked, 4> other;
  other.emplace_back(99, &alive);
  assert(alive == 4);

  other = std::move(dst);
  assert(other.size() == 3 && alive == 3);
  assert(other.front().value == 1 && other.back().value == 3);
  assert(dst.empty());

  return true;
}

constexpr bool test_inplace_vector_move_push() {
  int alive = 0;
  emb::inplace_vector<tracked, 2> v;

  tracked t{42, &alive};
  assert(alive == 1);

  v.push_back(std::move(t));
  assert(alive == 1);
  assert(v.back().value == 42);

  return true;
}

constexpr bool test_inplace_vector_try_pop() {
  int alive = 0;
  emb::inplace_vector<tracked, 2> v;

  auto empty_pop = v.try_pop_back();
  assert(!empty_pop.has_value());

  v.emplace_back(7, &alive);
  assert(alive == 1);

  auto popped = v.try_pop_back();
  assert(popped.has_value());
  assert(popped->value == 7);
  assert(v.empty());
  assert(alive == 1);

  return true;
}

constexpr bool test_inplace_vector_try_push() {
  emb::inplace_vector<int, 2> v;
  assert(v.try_push_back(1));
  assert(v.try_push_back(2));
  assert(!v.try_push_back(3));
  assert(v.size() == 2 && v.back() == 2 && v.front() == 1);

  return true;
}

struct no_default {
  int value;
  constexpr no_default(int v) : value(v) {}
};

static_assert(!std::is_default_constructible_v<no_default>);

constexpr bool test_inplace_vector_no_default_ctor() {
  emb::inplace_vector<no_default, 3> v;
  v.emplace_back(10);
  v.emplace_back(20);
  v.emplace_back(30);
  assert(v.full());
  assert(v.front().value == 10);
  assert(v.back().value == 30);
  assert(v[1].value == 20);

  v.pop_back();
  assert(v.back().value == 20);

  return true;
}

static_assert(test_inplace_vector_lifecycle());
static_assert(test_inplace_vector_copy());
static_assert(test_inplace_vector_move());
static_assert(test_inplace_vector_move_push());
static_assert(test_inplace_vector_try_pop());
static_assert(test_inplace_vector_try_push());
static_assert(test_inplace_vector_no_default_ctor());

} // namespace tests
} // namespace internal
} // namespace emb
