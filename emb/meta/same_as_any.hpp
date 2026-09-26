#pragma once

#include <concepts>

namespace emb {

// The concept `same_as_any<T, Ts...>` is satisfied if and only if `T` is the
// same type as one of the types in `Ts`, taking into account cv-qualification
// and references. It is not satisfied if `Ts` is empty.
template<typename T, typename... Ts>
concept same_as_any = (... || std::same_as<T, Ts>);

} // namespace emb
