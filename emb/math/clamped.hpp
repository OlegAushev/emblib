#pragma once

#include <emb/math/saturation.hpp>

#include <algorithm>
#include <compare>
#include <concepts>

namespace emb {

// The class template `clamped` holds a value of type `value_type`, the type
// of the bounds `lo` and `hi`, and keeps it in [`lo`, `hi`]. Constructing a
// `clamped` from a `value_type` clamps that value to [`lo`, `hi`], and a
// default-constructed `clamped` holds the value in [`lo`, `hi`] that is
// closest to zero. The sum and the difference of two `clamped` objects of the
// same type are clamped the same way, and so are, for a floating-point
// `value_type` only, the product and the quotient of a `clamped` and a
// `value_type`.
//
// For an integral `value_type`, a sum or difference is clamped as if computed
// exactly, so it neither overflows nor wraps around. The behavior is
// undefined if a value to be clamped, e.g. the argument of the constructor,
// is NaN.
//
// `clamped` is trivially copyable and has the size and alignment of
// `value_type`. The program is ill-formed if `lo` is not less than `hi`.
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

  constexpr value_type value() const
  {
    return v_;
  }

  constexpr clamped& operator+=(clamped const& rhs)
  {
    if constexpr (std::integral<value_type>) {
      v_ = std::clamp(saturating_add(v_, rhs.v_), lo, hi);
    }
    else {
      v_ = std::clamp(v_ + rhs.v_, lo, hi);
    }
    return *this;
  }

  constexpr clamped& operator-=(clamped const& rhs)
  {
    if constexpr (std::integral<value_type>) {
      v_ = std::clamp(saturating_sub(v_, rhs.v_), lo, hi);
    }
    else {
      v_ = std::clamp(v_ - rhs.v_, lo, hi);
    }
    return *this;
  }

  friend constexpr auto operator<=>(clamped const&, clamped const&) = default;

  friend constexpr clamped operator+(clamped const& lhs, clamped const& rhs)
  {
    clamped tmp = lhs;
    return tmp += rhs;
  }

  friend constexpr clamped operator-(clamped const& lhs, clamped const& rhs)
  {
    clamped tmp = lhs;
    return tmp -= rhs;
  }

  // scaling by a scalar requires a rounding policy for integral types,
  // which this class does not impose; floating-point only
  friend constexpr clamped operator*(clamped const& lhs, value_type rhs)
    requires std::floating_point<value_type>
  {
    return clamped(lhs.value() * rhs);
  }

  friend constexpr clamped operator*(value_type lhs, clamped const& rhs)
    requires std::floating_point<value_type>
  {
    return rhs * lhs;
  }

  friend constexpr clamped operator/(clamped const& lhs, value_type rhs)
    requires std::floating_point<value_type>
  {
    return clamped(lhs.value() / rhs);
  }
};

// `signed_pu` is an alias template for `clamped` with the bounds -1 and 1 of
// type `T`, for a per-unit value, i.e. a quantity expressed as a fraction of a
// base value.
template<std::floating_point T>
using signed_pu = clamped<T{-1}, T{1}>;

// `unsigned_pu` is an alias template for `clamped` with the bounds 0 and 1 of
// type `T`, for a per-unit value, i.e. a quantity expressed as a fraction of a
// base value.
template<std::floating_point T>
using unsigned_pu = clamped<T{0}, T{1}>;

// `signed_pu_f32` is `signed_pu` with values of type `float`.
using signed_pu_f32 = signed_pu<float>;

// `unsigned_pu_f32` is `unsigned_pu` with values of type `float`.
using unsigned_pu_f32 = unsigned_pu<float>;

} // namespace emb
