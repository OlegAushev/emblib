#include <cassert>

#include <emb/expected.hpp>

namespace {

enum class error { one, two };

// error type constructible from `error`, for cross-error-type propagation
struct wide_error {
  error err;
  constexpr wide_error(error e) : err(e) {}
};

constexpr auto make(std::expected<int, error> e) { return e; }

constexpr auto sum(std::expected<int, error> a, std::expected<int, error> b)
    -> std::expected<int, error> {
  return TRY(a) + TRY(b);
}

constexpr bool test_value() {
  auto r = sum(1, 2);
  assert(r.has_value() && *r == 3);
  return true;
}

[[maybe_unused]] constexpr bool test_error_propagation() {
  auto r = sum(1, std::unexpected(error::two));
  assert(!r.has_value() && r.error() == error::two);

  // first failure wins
  auto r2 = sum(std::unexpected(error::one), std::unexpected(error::two));
  assert(!r2.has_value() && r2.error() == error::one);
  return true;
}

[[maybe_unused]] constexpr bool test_lvalue_not_consumed() {
  auto e = make(42);
  auto f = [](std::expected<int, error>& x) -> std::expected<int, error> {
    return TRY(x);
  };
  auto r = f(e);
  assert(r.has_value() && *r == 42);
  assert(e.has_value() && *e == 42);
  return true;
}

constexpr auto run(std::expected<void, error> step, int& counter)
    -> std::expected<void, error> {
  TRY(step);
  ++counter;
  return {};
}

[[maybe_unused]] constexpr bool test_void() {
  int counter = 0;

  auto r = run({}, counter);
  assert(r.has_value() && counter == 1);

  auto r2 = run(std::unexpected(error::one), counter);
  assert(!r2.has_value() && r2.error() == error::one && counter == 1);
  return true;
}

constexpr auto widen(std::expected<int, error> e)
    -> std::expected<int, wide_error> {
  return TRY(e);
}

[[maybe_unused]] constexpr bool test_error_conversion() {
  auto r = widen(std::unexpected(error::two));
  assert(!r.has_value() && r.error().err == error::two);
  return true;
}

struct move_only {
  int val;
  constexpr explicit move_only(int v) : val(v) {}
  move_only(move_only const&) = delete;
  constexpr move_only(move_only&&) = default;
};

constexpr auto forward_move_only(int v) -> std::expected<move_only, error> {
  return TRY([](int x) -> std::expected<move_only, error> {
    return move_only{x};
  }(v));
}

constexpr bool test_rvalue_moved() {
  auto r = forward_move_only(7);
  assert(r.has_value() && r->val == 7);
  return true;
}

static_assert(test_value());
static_assert(test_rvalue_moved());
// The error path is not constant-evaluatable everywhere (see
// emb/expected.hpp); these cover it where possible.
#ifndef __clang__
static_assert(test_void());
#if __GNUC__ >= 16
static_assert(test_error_propagation());
static_assert(test_lvalue_not_consumed());
static_assert(test_error_conversion());
#endif
#endif

} // namespace
