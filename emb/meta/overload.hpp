#pragma once

namespace emb {

// The class template `overload` is a function object that combines the
// function call operators (the handlers) of the classes `Ts`, e.g. the types
// of lambdas, into one overload set. It is constructed from objects of `Ts`,
// e.g. `overload{[](int) {}, [](float) {}}`, for use as the visitor of
// `std::visit`.
//
// A catch-all overload makes a call with one argument ill-formed, with the
// diagnostic "Unsupported type", unless a handler takes the argument's own
// type, by value or by a reference that can bind to the argument: e.g. a
// `short` does not reach a handler that takes `int`, and a const `int` does
// not reach one that takes `int&`. The call is ill-formed even in an
// unevaluated operand: `std::visit` gives this diagnostic for an alternative
// that no handler takes, whatever the handlers return, and a test for a
// handler, e.g. `std::is_invocable`, is ill-formed rather than `false`. A
// generic lambda whose parameter is `auto` or a reference to `auto`, e.g.
// `[](auto const&) {}`, is never selected for one argument unless it takes a
// constrained `auto` by value; a call that no other handler takes is then
// ambiguous or selects the catch-all. A call with any other number of
// arguments, e.g. from `std::visit` over two variants, is resolved among the
// handlers alone, conversions included.
template<typename... Ts>
struct overload : Ts... {
  using Ts::operator()...;

  consteval auto operator()(auto) const
  {
    static_assert(false, "Unsupported type");
  }
};

} // namespace emb
