#pragma once

#include <emblib/core.hpp>
#include <emblib/math.hpp>

namespace emb {
namespace units {

template<typename T, typename Unit>
class named_unit {
private:
  T v_;
public:
  named_unit() : v_(T(0)) {}

  EMB_CONSTEXPR explicit named_unit(T const& v) : v_(v) {}

  EMB_CONSTEXPR T const& numval() const { return v_; }

  EMB_CONSTEXPR named_unit& operator+=(named_unit const& rhs) {
    v_ += rhs.v_;
    return *this;
  }

  EMB_CONSTEXPR named_unit& operator-=(named_unit const& rhs) {
    v_ -= rhs.v_;
    return *this;
  }
};

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR bool
operator==(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  return lhs.numval() == rhs.numval();
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR bool
operator!=(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  return lhs.numval() != rhs.numval();
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR bool
operator<(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  return lhs.numval() < rhs.numval();
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR bool
operator>(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  return lhs.numval() > rhs.numval();
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR bool
operator<=(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  return lhs.numval() <= rhs.numval();
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR bool
operator>=(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  return lhs.numval() >= rhs.numval();
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR named_unit<T, Unit>
operator+(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  named_unit<T, Unit> tmp = lhs;
  return tmp += rhs;
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR named_unit<T, Unit>
operator-(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  named_unit<T, Unit> tmp = lhs;
  return tmp -= rhs;
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR named_unit<T, Unit>
operator*(named_unit<T, Unit> const& lhs, float rhs) {
  return named_unit<T, Unit>(lhs.numval() * rhs);
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR named_unit<T, Unit>
operator*(float lhs, named_unit<T, Unit> const& rhs) {
  return rhs * lhs;
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR named_unit<T, Unit>
operator/(named_unit<T, Unit> const& lhs, float rhs) {
  return named_unit<T, Unit>(lhs.numval() / rhs);
}

namespace impl {
// speed
struct rpm {};

struct eradps {};

// angle
struct erad {};

struct edeg {};

struct rad {};

struct deg {};
} // namespace impl

typedef named_unit<float, impl::rpm> rpm_t;
typedef named_unit<float, impl::eradps> eradps_t;

typedef named_unit<float, impl::erad> erad_t;
typedef named_unit<float, impl::edeg> edeg_t;
typedef named_unit<float, impl::rad> rad_t;
typedef named_unit<float, impl::deg> deg_t;

EMB_INLINE_CONSTEXPR eradps_t to_eradps(rpm_t n, int p) {
  return eradps_t(emb::to_eradps(n.numval(), p));
}

EMB_INLINE_CONSTEXPR rpm_t to_rpm(eradps_t w, int p) {
  return rpm_t(emb::to_rpm(w.numval(), p));
}

class motorspeed_t {
private:
  int p_;
  eradps_t w_;
public:
  EMB_CONSTEXPR explicit motorspeed_t(int p) : p_(p), w_(0) {}

  EMB_CONSTEXPR motorspeed_t(int p, eradps_t w) : p_(p) { set(w); }

  EMB_CONSTEXPR motorspeed_t(int p, rpm_t n) : p_(p) { set(n); }

  template<typename Unit>
  EMB_CONSTEXPR motorspeed_t& operator=(Unit v) {
    set(v);
    return *this;
  }

  EMB_CONSTEXPR int p() const { return p_; }

  EMB_CONSTEXPR eradps_t eradps() const { return w_; }

  EMB_CONSTEXPR rpm_t rpm() const { return to_rpm(w_, p_); }
private:
  EMB_CONSTEXPR void set(eradps_t w) { w_ = w; }

  EMB_CONSTEXPR void set(rpm_t n) { w_ = to_eradps(n, p_); }
};

EMB_INLINE_CONSTEXPR motorspeed_t
operator*(motorspeed_t const& lhs, float rhs) {
  return motorspeed_t(lhs.p(), lhs.eradps() * rhs);
}

EMB_INLINE_CONSTEXPR motorspeed_t
operator*(float lhs, motorspeed_t const& rhs) {
  return rhs * lhs;
}

EMB_INLINE_CONSTEXPR motorspeed_t
operator/(motorspeed_t const& lhs, float rhs) {
  return motorspeed_t(lhs.p(), lhs.eradps() / rhs);
}

class eangle_t {
private:
  erad_t erad_;
public:
  EMB_CONSTEXPR eangle_t() : erad_(0) {}

  EMB_CONSTEXPR explicit eangle_t(erad_t v) { set(v); }

  EMB_CONSTEXPR explicit eangle_t(edeg_t v) { set(v); }

  template<typename Unit>
  EMB_CONSTEXPR eangle_t& operator=(Unit v) {
    set(v);
    return *this;
  }

  EMB_CONSTEXPR erad_t erad() const { return erad_; }

  EMB_CONSTEXPR edeg_t edeg() const { return edeg_t(to_deg(erad_.numval())); }
private:
  EMB_CONSTEXPR void set(erad_t v) { erad_ = v; }

  EMB_CONSTEXPR void set(edeg_t v) { erad_ = erad_t(to_rad(v.numval())); }
};

class angle_t {
private:
  rad_t rad_;
public:
  EMB_CONSTEXPR angle_t() : rad_(0) {}

  EMB_CONSTEXPR explicit angle_t(rad_t v) { set(v); }

  EMB_CONSTEXPR explicit angle_t(deg_t v) { set(v); }

  template<typename Unit>
  EMB_CONSTEXPR angle_t& operator=(Unit v) {
    set(v);
    return *this;
  }

  EMB_CONSTEXPR rad_t rad() const { return rad_; }

  EMB_CONSTEXPR deg_t deg() const { return deg_t(to_deg(rad_.numval())); }
private:
  EMB_CONSTEXPR void set(rad_t v) { rad_ = v; }

  EMB_CONSTEXPR void set(deg_t v) { rad_ = rad_t(to_rad(v.numval())); }
};

} // namespace units
} // namespace emb

template<typename T, typename Unit>
inline emb::units::named_unit<T, Unit>
abs(emb::units::named_unit<T, Unit> const& v) {
  return emb::units::named_unit<T, Unit>(abs(v.numval()));
}
