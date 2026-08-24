#pragma once

#include <concepts>
#include <numeric>

// C++26 saturation arithmetic was renamed between standard revisions
// (add_sat -> saturating_add etc.); libstdc++ 15 and 16 ship different
// names. These shims quarantine the churn until toolchains converge.

namespace emb {

template<std::integral T>
constexpr T saturating_add(T x, T y) {
#if __cpp_lib_saturation_arithmetic >= 202603L
  return std::saturating_add(x, y);
#else
  return std::add_sat(x, y);
#endif
}

template<std::integral T>
constexpr T saturating_sub(T x, T y) {
#if __cpp_lib_saturation_arithmetic >= 202603L
  return std::saturating_sub(x, y);
#else
  return std::sub_sat(x, y);
#endif
}

template<std::integral R, std::integral T>
constexpr R saturating_cast(T x) {
#if __cpp_lib_saturation_arithmetic >= 202603L
  return std::saturating_cast<R>(x);
#else
  return std::saturate_cast<R>(x);
#endif
}

} // namespace emb
