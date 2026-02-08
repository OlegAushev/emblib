#pragma once

#include <utility>

namespace emb {

template<class T>
inline T median_of_three(T a, T b, T c) {
  if (a > c) {
    std::swap(a, c);
  }
  if (a > b) {
    std::swap(a, b);
  }
  if (b > c) {
    std::swap(b, c);
  }
  return b;
}

} // namespace emb
