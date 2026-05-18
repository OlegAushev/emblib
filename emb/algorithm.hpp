#pragma once

#include <array>
#include <utility>

namespace emb {

template<class T>
constexpr T median3(T a, T b, T c) noexcept {
  if (a > b) std::swap(a, b);
  if (b > c) std::swap(b, c);
  if (a > b) std::swap(a, b);
  return b;
}

template<class T>
constexpr T median3(std::array<T, 3> v) noexcept {
  return median3(v[0], v[1], v[2]);
}

} // namespace emb
