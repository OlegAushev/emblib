#pragma once

#include <concepts>

namespace emb::signal {

// Rules out the unsigned integers, where -x is a wraparound or a promotion to
// another type: a raw code, which is what a path carries past its last stage
// and the one place a sign convention has nothing to say.
template<typename T>
concept sign_reversible = requires(T x) { { -x } -> std::same_as<T>; }
                       && !std::unsigned_integral<T>;

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
