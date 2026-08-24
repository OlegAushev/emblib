#pragma once

#include <algorithm>
#include <compare>

namespace emb {

template<float lo, float hi>
class clamped {
  static_assert(lo < hi);
private:
  float v_;
public:
  constexpr clamped() : v_(std::clamp(0.0f, lo, hi)) {}

  constexpr explicit clamped(float v) : v_(std::clamp(v, lo, hi)) {}

  constexpr float value() const {
    return v_;
  }

  constexpr clamped& operator+=(clamped const& rhs) {
    v_ = std::clamp(v_ + rhs.v_, lo, hi);
    return *this;
  }

  constexpr clamped& operator-=(clamped const& rhs) {
    v_ = std::clamp(v_ - rhs.v_, lo, hi);
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

  friend constexpr clamped operator*(clamped const& lhs, float rhs) {
    return clamped(lhs.value() * rhs);
  }

  friend constexpr clamped operator*(float lhs, clamped const& rhs) {
    return rhs * lhs;
  }

  friend constexpr clamped operator/(clamped const& lhs, float rhs) {
    return clamped(lhs.value() / rhs);
  }
};

using signed_pu = clamped<-1.0f, 1.0f>;
using unsigned_pu = clamped<0.0f, 1.0f>;

} // namespace emb
