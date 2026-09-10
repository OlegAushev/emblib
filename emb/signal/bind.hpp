#pragma once

namespace emb::signal {

// Lifts a constexpr object of structural type -- a transducer model, a
// mounting orientation -- into an empty stage type, so it composes into a
// default-constructed path next to stateless stages. What it binds is fixed
// at compile time, unlike a stage that carries state estimated at run time,
// such as a zero trim.
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
