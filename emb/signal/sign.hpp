#pragma once

#include <concepts>

namespace emb::signal {

// The concept `sign_reversible<T>` is satisfied if and only if `T` is not an
// unsigned integer type and `-x`, for an lvalue `x` of type `T`, is a valid
// expression of type `T`. Every specialization of `emb::units::named_unit`
// satisfies it, and so do `int`, `long`, `long long` and the floating-point
// types, but not their cv-qualified versions or references to them.
// `signed char`, `short` and `bool` do not satisfy it, since `-x` promotes them
// to `int`.
//
// `negation` requires it of its arguments. The concept rules out raw codes of
// an unsigned type: a raw code, i.e. what a path carries past its last stage,
// has no sign to reverse, and on an unsigned integer `-x` is a wraparound or a
// promotion to another type.
template<typename T>
concept sign_reversible = requires(T x) {
  { -x } -> std::same_as<T>;
} && !std::unsigned_integral<T>;

// `identity` is a stage whose `forward` and `inverse` both return their
// argument unchanged, whatever its type, a raw code included. With `negation`,
// it forms the pair of stages that carry only a sign: a path parameterized
// over its sign convention takes `identity` to keep the sign and `negation` to
// reverse it. As an empty class, `identity` composes into a path without
// `bind`.
struct identity {
  static constexpr auto forward(auto x)
  {
    return x;
  }

  static constexpr auto inverse(auto x)
  {
    return x;
  }
};

// `negation` is a stage that reverses the sign of its input: `forward` and
// `inverse` both return `-x` for their argument `x`, so the stage is its own
// inverse. The result has the type of `x`, so a quantity keeps its unit. A
// path parameterized over its sign convention takes `negation` to reverse the
// sign, e.g. for a transducer mounted against the direction it measures, an
// encoder counting the other way or a swapped pair of leads, and `identity` to
// keep it. As an empty class, `negation` composes into a path without `bind`.
//
// A path that passes a raw code of an unsigned type to a `negation` fails to
// compile, since no unsigned integer type satisfies `sign_reversible`. The
// behavior of `forward` and `inverse` is undefined if the argument is of a
// signed integer type and is the minimum value of that type, e.g. `INT_MIN`
// for `int`.
struct negation {
  static constexpr auto forward(sign_reversible auto x)
  {
    return -x;
  }

  static constexpr auto inverse(sign_reversible auto x)
  {
    return -x;
  }
};

} // namespace emb::signal
