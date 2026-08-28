#pragma once

#include <concepts>

namespace emb {

template<typename T, typename... Ts>
concept same_as_any = (... || std::same_as<T, Ts>);

} // namespace emb
