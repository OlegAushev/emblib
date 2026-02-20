#pragma once

#include <emb/meta/type_index.hpp>
#include <emb/meta/typelist.hpp>
#include <emb/meta/unroll.hpp>

#include <concepts>

namespace emb {

template<typename T, typename... Ts>
concept same_as_any = (... || std::same_as<T, Ts>);

template<typename... Ts>
struct overload : Ts... {
  using Ts::operator()...;

  consteval void operator()(auto) const {
    static_assert(false, "Unsupported type");
  }
};

} // namespace emb
