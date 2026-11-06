#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace emb {

// Returns the median of `a`, `b` and `c`. None of them may be NaN.
template<class T>
constexpr T median(T a, T b, T c) noexcept
{
  return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

// Returns the median of `a`, `b`, `c`, `d` and `e`. None of them may be NaN.
template<class T>
constexpr T median(T a, T b, T c, T d, T e) noexcept
{
  if (b < a) std::swap(a, b);
  if (e < d) std::swap(d, e);
  return median(std::max(a, d), c, std::min(b, e));
}

// Returns the median of `a`, `b`, `c`, `d`, `e`, `f` and `g`. None of them may
// be NaN.
template<class T>
constexpr T median(T a, T b, T c, T d, T e, T f, T g) noexcept
{
  if (b < a) std::swap(a, b);
  if (c < b) std::swap(b, c);
  if (b < a) std::swap(a, b);
  if (e < d) std::swap(d, e);
  if (g < f) std::swap(f, g);
  return median(std::max(a, d), median(b, e, f), std::min(c, g));
}

// Returns the median of `values`, whose number must be odd: an even number of
// values has no single median. Three, five and seven values go to the overloads
// above, and any other number is sorted in a copy, which takes O(N log N)
// comparisons. The values must not be NaN: with a NaN and two unequal numbers
// among them, `<` is not the strict weak ordering that `std::sort` requires,
// and the behavior is undefined.
template<class T, std::size_t N>
  requires(N % 2 == 1)
constexpr T median(std::array<T, N> const& values)
{
  if constexpr (N == 3) {
    return median(values[0], values[1], values[2]);
  }
  else if constexpr (N == 5) {
    return median(values[0], values[1], values[2], values[3], values[4]);
  }
  else if constexpr (N == 7) {
    return median(values[0],
                  values[1],
                  values[2],
                  values[3],
                  values[4],
                  values[5],
                  values[6]);
  }
  else {
    auto sorted = values;
    std::sort(sorted.begin(), sorted.end());
    return sorted[N / 2];
  }
}

} // namespace emb
