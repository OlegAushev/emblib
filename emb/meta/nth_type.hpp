#pragma once

#include <cstddef>

namespace emb {

// Provides the member typedef `type`, which is the type at zero-based index
// `I` in `Ts`. The program is ill-formed if `I >= sizeof...(Ts)`.
//
// GCC cannot mangle the builtin `__type_pack_element`, so the builtin must not
// appear in the signature of a function template. An alias template is
// transparent to mangling, so `nth_type_t` names the member `type` rather than
// the builtin. For the same reason `nth_type_t` cannot name the pack indexing
// specifier `Ts...[I]`, which GCC 15 cannot mangle either.
template<std::size_t I, typename... Ts>
struct nth_type {
  using type = __type_pack_element<I, Ts...>;
};

// Helper alias template for `nth_type`.
template<std::size_t I, typename... Ts>
using nth_type_t = typename nth_type<I, Ts...>::type;

} // namespace emb
