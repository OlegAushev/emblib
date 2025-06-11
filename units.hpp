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

  explicit named_unit(T const& v) : v_(v) {}

  T const& numval() const { return v_; }

  named_unit& operator+=(named_unit const& rhs) {
    v_ += rhs.v_;
    return *this;
  }

  named_unit& operator-=(named_unit const& rhs) {
    v_ -= rhs.v_;
    return *this;
  }
};

template<typename T, typename Unit>
inline bool operator==(named_unit<T, Unit> const& lhs,
                       named_unit<T, Unit> const& rhs) {
  return lhs.numval() == rhs.numval();
}

template<typename T, typename Unit>
inline bool operator!=(named_unit<T, Unit> const& lhs,
                       named_unit<T, Unit> const& rhs) {
  return lhs.numval() != rhs.numval();
}

template<typename T, typename Unit>
inline bool operator<(named_unit<T, Unit> const& lhs,
                      named_unit<T, Unit> const& rhs) {
  return lhs.numval() < rhs.numval();
}

template<typename T, typename Unit>
inline bool operator>(named_unit<T, Unit> const& lhs,
                      named_unit<T, Unit> const& rhs) {
  return lhs.numval() > rhs.numval();
}

template<typename T, typename Unit>
inline bool operator<=(named_unit<T, Unit> const& lhs,
                       named_unit<T, Unit> const& rhs) {
  return lhs.numval() <= rhs.numval();
}

template<typename T, typename Unit>
inline bool operator>=(named_unit<T, Unit> const& lhs,
                       named_unit<T, Unit> const& rhs) {
  return lhs.numval() >= rhs.numval();
}

template<typename T, typename Unit>
inline named_unit<T, Unit> operator+(named_unit<T, Unit> const& lhs,
                                     named_unit<T, Unit> const& rhs) {
  named_unit<T, Unit> tmp = lhs;
  return tmp += rhs;
}

template<typename T, typename Unit>
inline named_unit<T, Unit> operator-(named_unit<T, Unit> const& lhs,
                                     named_unit<T, Unit> const& rhs) {
  named_unit<T, Unit> tmp = lhs;
  return tmp -= rhs;
}

template<typename T, typename Unit>
inline named_unit<T, Unit> operator*(named_unit<T, Unit> const& lhs,
                                     float rhs) {
  return named_unit<T, Unit>(lhs.numval() * rhs);
}

template<typename T, typename Unit>
inline named_unit<T, Unit> operator*(float lhs,
                                     named_unit<T, Unit> const& rhs) {
  return rhs * lhs;
}

template<typename T, typename Unit>
inline named_unit<T, Unit> operator/(named_unit<T, Unit> const& lhs,
                                     float rhs) {
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

EMB_CONSTEXPR eradps_t to_eradps(rpm_t n, int p) {
  return eradps_t(emb::to_eradps(n.numval(), p));
}

EMB_CONSTEXPR rpm_t to_rpm(eradps_t w, int p) {
  return rpm_t(emb::to_rpm(w.numval(), p));
}

class motorspeed_t {
private:
  int p_;
  eradps_t w_;
public:
  explicit motorspeed_t(int p) : p_(p), w_(0) {}

  motorspeed_t(int p, eradps_t w) : p_(p) { set(w); }

  motorspeed_t(int p, rpm_t n) : p_(p) { set(n); }

  template<typename Unit>
  motorspeed_t& operator=(Unit v) {
    set(v);
    return *this;
  }

  int p() const { return p_; }

  eradps_t eradps() const { return w_; }

  rpm_t rpm() const { return to_rpm(w_, p_); }
private:
  void set(eradps_t w) { w_ = w; }

  void set(rpm_t n) { w_ = to_eradps(n, p_); }
};

inline motorspeed_t operator*(motorspeed_t const& lhs, float rhs) {
  return motorspeed_t(lhs.p(), lhs.eradps() * rhs);
}

inline motorspeed_t operator*(float lhs, motorspeed_t const& rhs) {
  return rhs * lhs;
}

inline motorspeed_t operator/(motorspeed_t const& lhs, float rhs) {
  return motorspeed_t(lhs.p(), lhs.eradps() / rhs);
}

class eangle_t {
private:
  erad_t erad_;
public:
  eangle_t() : erad_(0) {}

  explicit eangle_t(erad_t v) { set(v); }

  explicit eangle_t(edeg_t v) { set(v); }

  template<typename Unit>
  eangle_t& operator=(Unit v) {
    set(v);
    return *this;
  }

  erad_t erad() const { return erad_; }

  edeg_t edeg() const { return edeg_t(to_deg(erad_.numval())); }
private:
  void set(erad_t v) { erad_ = v; }

  void set(edeg_t v) { erad_ = erad_t(to_rad(v.numval())); }
};

class angle_t {
private:
  rad_t rad_;
public:
  angle_t() : rad_(0) {}

  explicit angle_t(rad_t v) { set(v); }

  explicit angle_t(deg_t v) { set(v); }

  template<typename Unit>
  angle_t& operator=(Unit v) {
    set(v);
    return *this;
  }

  rad_t rad() const { return rad_; }

  deg_t deg() const { return deg_t(to_deg(rad_.numval())); }
private:
  void set(rad_t v) { rad_ = v; }

  void set(deg_t v) { rad_ = rad_t(to_rad(v.numval())); }
};

} // namespace units
} // namespace emb

template<typename T, typename Unit>
inline emb::units::named_unit<T, Unit>
abs(emb::units::named_unit<T, Unit> const& v) {
  return emb::units::named_unit<T, Unit>(abs(v.numval()));
}
