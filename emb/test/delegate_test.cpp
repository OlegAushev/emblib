#include <emb/delegate.hpp>

#include <cassert>
#include <type_traits>

namespace {

constexpr int add(int a, int b)
{
  return a + b;
}

constexpr int mul(int a, int b) noexcept
{
  return a * b;
}

class foo {
  int a_;
  int b_;
public:
  constexpr foo(int a, int b) : a_(a), b_(b) {}

  constexpr int add(int c)
  {
    return a_ + b_ + c;
  }

  constexpr int sub(int c)
  {
    return a_ + b_ - c;
  }

  constexpr int mul(int c) noexcept
  {
    return (a_ + b_) * c;
  }

  constexpr int sum() const
  {
    return a_ + b_;
  }

  constexpr int product() const noexcept
  {
    return a_ * b_;
  }
};

constexpr bool test_delegate()
{
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

// bind() reference overloads
constexpr bool test_bind_from_reference()
{
  foo f(-1, 5);
  [[maybe_unused]] auto add_delegate =
      emb::delegate<int(int)>::bind<&foo::add>(f);
  assert(add_delegate(6) == 10);

  foo const cf(1, 3);
  [[maybe_unused]] auto sum_delegate =
      emb::delegate<int()>::bind<&foo::sum>(cf);
  assert(sum_delegate() == 4);

  return true;
}

static_assert(test_bind_from_reference());

// make_delegate() signature deduction and binding
constexpr bool test_make_delegate()
{
  auto add_delegate = emb::make_delegate<&add>();
  static_assert(
      std::is_same_v<decltype(add_delegate), emb::delegate<int(int, int)>>);
  assert(add_delegate(1, 2) == 3);

  foo f(-1, 5);
  [[maybe_unused]] auto add_via_ptr = emb::make_delegate<&foo::add>(&f);
  auto add_via_ref = emb::make_delegate<&foo::add>(f);
  static_assert(std::is_same_v<decltype(add_via_ref), emb::delegate<int(int)>>);
  assert(add_via_ptr(6) == 10);
  assert(add_via_ref(6) == 10);

  foo const cf(1, 3);
  auto sum_delegate = emb::make_delegate<&foo::sum>(cf);
  static_assert(std::is_same_v<decltype(sum_delegate), emb::delegate<int()>>);
  assert(sum_delegate() == 4);

  // noexcept is part of the function type but not of the deduced signature
  auto mul_delegate = emb::make_delegate<&mul>();
  static_assert(
      std::is_same_v<decltype(mul_delegate), emb::delegate<int(int, int)>>);
  assert(mul_delegate(2, 3) == 6);

  auto mul_via_ref = emb::make_delegate<&foo::mul>(f);
  static_assert(std::is_same_v<decltype(mul_via_ref), emb::delegate<int(int)>>);
  assert(mul_via_ref(2) == 8);

  auto product_delegate = emb::make_delegate<&foo::product>(cf);
  static_assert(
      std::is_same_v<decltype(product_delegate), emb::delegate<int()>>);
  assert(product_delegate() == 3);

  return true;
}

static_assert(test_make_delegate());

// operator bool, operator==/!= and empty state
constexpr bool test_comparison()
{
  [[maybe_unused]] emb::delegate<int(int)> empty;
  assert(!empty);
  assert(empty == emb::delegate<int(int)>());

  foo f1(1, 2);
  foo f2(3, 4);

  [[maybe_unused]] auto add_f1 = emb::make_delegate<&foo::add>(f1);
  [[maybe_unused]] auto add_f1_again = emb::make_delegate<&foo::add>(f1);
  [[maybe_unused]] auto add_f2 = emb::make_delegate<&foo::add>(f2);
  [[maybe_unused]] auto sub_f1 = emb::make_delegate<&foo::sub>(f1);

  assert(static_cast<bool>(add_f1));
  assert(add_f1 == add_f1_again); // same object, same target
  assert(add_f1 != add_f2);       // different object
  assert(add_f1 != sub_f1);       // same object, different target
  assert(add_f1 != empty);

  return true;
}

static_assert(test_comparison());

// a void delegate discards the target's return value
constexpr bool test_void_return_discarded()
{
  foo f(1, 2);
  auto add_delegate = emb::delegate<void(int)>::bind<&foo::add>(f);
  add_delegate(3);
  auto sum_delegate = emb::delegate<void()>::bind<&foo::sum>(f);
  sum_delegate();
  auto free_delegate = emb::delegate<void(int, int)>::bind<&add>();
  free_delegate(1, 2);
  return true;
}

static_assert(test_void_return_discarded());

// a free function that takes the bound object as its first argument
constexpr int scaled_sum(foo const* f, int k)
{
  return f->sum() * k;
}

constexpr bool test_free_function_with_context()
{
  foo const f(1, 2);
  [[maybe_unused]] auto scaled_delegate =
      emb::delegate<int(int)>::bind<&scaled_sum>(f);
  assert(scaled_delegate(3) == 9);
  return true;
}

static_assert(test_free_function_with_context());

// binding to a temporary must be rejected at compile time
template<typename T>
concept bind_accepts = requires(T&& obj) {
  emb::delegate<int(int)>::bind<&foo::add>(static_cast<T&&>(obj));
};

template<typename T>
concept make_delegate_accepts =
    requires(T&& obj) { emb::make_delegate<&foo::add>(static_cast<T&&>(obj)); };

static_assert(bind_accepts<foo&>);
static_assert(!bind_accepts<foo>);
static_assert(!bind_accepts<foo const>);

static_assert(make_delegate_accepts<foo&>);
static_assert(!make_delegate_accepts<foo>);
static_assert(!make_delegate_accepts<foo const>);

} // namespace
