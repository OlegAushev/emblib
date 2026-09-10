#pragma once

namespace emb::signal {

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
  static constexpr auto forward(auto x)
  {
    return -x;
  }

  static constexpr auto inverse(auto x)
  {
    return -x;
  }
};

} // namespace emb::signal
