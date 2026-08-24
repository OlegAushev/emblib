#pragma once

#include <emb/math/saturation.hpp>

#include <algorithm>
#include <compare>
#include <concepts>

namespace emb {

template<auto lo, decltype(lo) hi>
  requires std::integral<decltype(lo)> || std::floating_point<decltype(lo)>
class clamped {
public:
  using value_type = decltype(lo);
private:
  static_assert(lo < hi);

  value_type v_;
public:
  constexpr clamped() : v_(std::clamp(value_type{0}, lo, hi)) {}

  constexpr explicit clamped(value_type v) : v_(std::clamp(v, lo, hi)) {}

  constexpr value_type value() const {
    return v_;
  }

  constexpr clamped& operator+=(clamped const& rhs) {
    if constexpr (std::integral<value_type>) {
      v_ = std::clamp(saturating_add(v_, rhs.v_), lo, hi);
    } else {
      v_ = std::clamp(v_ + rhs.v_, lo, hi);
    }
    return *this;
  }

  constexpr clamped& operator-=(clamped const& rhs) {
    if constexpr (std::integral<value_type>) {
      v_ = std::clamp(saturating_sub(v_, rhs.v_), lo, hi);
    } else {
      v_ = std::clamp(v_ - rhs.v_, lo, hi);
    }
    return *this;
  }

  friend constexpr auto operator<=>(clamped const&, clamped const&) = default;

  friend constexpr clamped operator+(clamped const& lhs, clamped const& rhs) {
    clamped tmp = lhs;
    return tmp += rhs;
  }

  friend constexpr clamped operator-(clamped const& lhs, clamped const& rhs) {
    clamped tmp = lhs;
    return tmp -= rhs;
  }

  // scaling by a scalar requires a rounding policy for integral types,
  // which this class does not impose; floating-point only
  friend constexpr clamped operator*(clamped const& lhs, value_type rhs)
    requires std::floating_point<value_type> {
    return clamped(lhs.value() * rhs);
  }

  friend constexpr clamped operator*(value_type lhs, clamped const& rhs)
    requires std::floating_point<value_type> {
    return rhs * lhs;
  }

  friend constexpr clamped operator/(clamped const& lhs, value_type rhs)
    requires std::floating_point<value_type> {
    return clamped(lhs.value() / rhs);
  }
};

using signed_pu = clamped<-1.0f, 1.0f>;
using unsigned_pu = clamped<0.0f, 1.0f>;

} // namespace emb
