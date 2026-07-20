#pragma once

#include <cstddef>

namespace emb {

// __type_pack_element instead of C++26 pack indexing: clang trips over pack
// indexing in dependent contexts. The builtin must be wrapped in a class so
// that it never appears in function signatures directly — GCC cannot mangle
// the bare builtin.
template<std::size_t I, typename... Ts>
struct nth_type {
  using type = __type_pack_element<I, Ts...>;
};

// Helper template
template<std::size_t I, typename... Ts>
using nth_type_t = typename nth_type<I, Ts...>::type;

} // namespace emb
