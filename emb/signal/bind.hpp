#pragma once

namespace emb::signal {

// The class template `bind` wraps `stage`, a constant object of a stage class,
// in an empty stage class whose static member functions `forward` and
// `inverse` apply the `forward` and `inverse` of `stage`. `bind<stage>` is
// default-constructible, so a stage whose parameters are data members, such
// as a `proportional`, composes into a default-constructed `path` when bound.
// `stage` is fixed at compile time, so a stage that carries run-time state,
// such as a zero trim, is held by value in the `path` instead.
//
// `stage` must be a constant expression, e.g. a `constexpr` variable, of a
// structural class type, i.e. a literal class type whose bases and non-static
// data members are all public, not `mutable`, and of structural types or
// arrays of them. Since `stage` is a `const` object, its `forward` and
// `inverse` must be `const` or static member functions.
template<auto stage>
struct bind {
  static constexpr auto forward(auto x)
  {
    return stage.forward(x);
  }

  static constexpr auto inverse(auto y)
  {
    return stage.inverse(y);
  }
};

} // namespace emb::signal
