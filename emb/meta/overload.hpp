#pragma once

namespace emb {

template<typename... Ts>
struct overload : Ts... {
  using Ts::operator()...;

  consteval void operator()(auto) const
  {
    static_assert(false, "Unsupported type");
  }
};

} // namespace emb
