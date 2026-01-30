#pragma once

#include <emb/meta/type_index.hpp>
#include <emb/meta/typelist.hpp>
#include <emb/meta/unroll.hpp>

#include <concepts>

namespace emb {

template<typename T, typename... Ts>
concept same_as_any = (... || std::same_as<T, Ts>);

} // namespace emb
