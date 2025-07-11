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

#ifdef __cpp_concepts

template<typename Unit>
concept RadianUnit = std::same_as<Unit, erad_t> || std::same_as<Unit, rad_t>;

template<typename Unit>
concept DegreeUnit = std::same_as<Unit, edeg_t> || std::same_as<Unit, deg_t>;

template<typename Unit>
concept EAngleUnit = std::same_as<Unit, erad_t> || std::same_as<Unit, edeg_t>;

template<typename Unit>
concept AngleUnit = std::same_as<Unit, rad_t> || std::same_as<Unit, deg_t>;

#endif

} // namespace units

#ifdef __cpp_concepts

template<units::RadianUnit Unit>
Unit rem2pi(Unit v) {
  return Unit{rem2pi(v.numval())};
}

template<units::RadianUnit Unit>
Unit rempi(Unit v) {
  return Unit{rempi(v.numval())};
}

template<units::DegreeUnit Unit>
Unit rem2pi(Unit v) {
  float v_ = fmodf(v.numval(), to_deg(numbers::two_pi));
  if (v_ < 0) {
    v_ += to_deg(numbers::two_pi);
  }
  return Unit{v_};
}

template<units::DegreeUnit Unit>
Unit rempi(Unit v) {
  float v_ = fmodf(v.numval() + to_deg(numbers::pi), to_deg(numbers::two_pi));
  if (v_ < 0) {
    v_ += to_deg(numbers::two_pi);
  }
  return Unit{v_ - to_deg(numbers::pi)};
}

#endif

EMB_INLINE_CONSTEXPR units::erad_t to_rad(units::edeg_t deg) {
  return units::erad_t(emb::to_rad(deg.numval()));
}

EMB_INLINE_CONSTEXPR units::edeg_t to_deg(units::erad_t rad) {
  return units::edeg_t(emb::to_deg(rad.numval()));
}

EMB_INLINE_CONSTEXPR units::rad_t to_rad(units::deg_t deg) {
  return units::rad_t(emb::to_rad(deg.numval()));
}

EMB_INLINE_CONSTEXPR units::deg_t to_deg(units::rad_t rad) {
  return units::deg_t(emb::to_deg(rad.numval()));
}

EMB_INLINE_CONSTEXPR units::eradps_t to_eradps(units::rpm_t n, int p) {
  return units::eradps_t(emb::to_eradps(n.numval(), p));
}

EMB_INLINE_CONSTEXPR units::rpm_t to_rpm(units::eradps_t w, int p) {
  return units::rpm_t(emb::to_rpm(w.numval(), p));
}

namespace units {

/*============================================================================*/
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

// NOTE: if lhs.p() != rhs.p() then comparison operator behaviour is undefined
EMB_INLINE_CONSTEXPR bool
operator==(motorspeed_t const& lhs, motorspeed_t const& rhs) {
  return lhs.eradps() == rhs.eradps();
}

EMB_INLINE_CONSTEXPR bool
operator!=(motorspeed_t const& lhs, motorspeed_t const& rhs) {
  return lhs.eradps() != rhs.eradps();
}

EMB_INLINE_CONSTEXPR bool
operator<(motorspeed_t const& lhs, motorspeed_t const& rhs) {
  return lhs.eradps() < rhs.eradps();
}

EMB_INLINE_CONSTEXPR bool
operator>(motorspeed_t const& lhs, motorspeed_t const& rhs) {
  return lhs.eradps() > rhs.eradps();
}

EMB_INLINE_CONSTEXPR bool
operator<=(motorspeed_t const& lhs, motorspeed_t const& rhs) {
  return lhs.eradps() <= rhs.eradps();
}

EMB_INLINE_CONSTEXPR bool
operator>=(motorspeed_t const& lhs, motorspeed_t const& rhs) {
  return lhs.eradps() >= rhs.eradps();
}

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

/*============================================================================*/

/*============================================================================*/
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

  EMB_CONSTEXPR eangle_t& operator+=(eangle_t const& rhs) {
    erad_ += rhs.erad_;
    return *this;
  }

  EMB_CONSTEXPR eangle_t& operator-=(eangle_t const& rhs) {
    erad_ -= rhs.erad_;
    return *this;
  }
private:
  EMB_CONSTEXPR void set(erad_t v) { erad_ = v; }

  EMB_CONSTEXPR void set(edeg_t v) { erad_ = erad_t(to_rad(v.numval())); }
};

EMB_INLINE_CONSTEXPR bool operator==(eangle_t const& lhs, eangle_t const& rhs) {
  return lhs.erad() == rhs.erad();
}

EMB_INLINE_CONSTEXPR bool operator!=(eangle_t const& lhs, eangle_t const& rhs) {
  return lhs.erad() != rhs.erad();
}

EMB_INLINE_CONSTEXPR bool operator<(eangle_t const& lhs, eangle_t const& rhs) {
  return lhs.erad() < rhs.erad();
}

EMB_INLINE_CONSTEXPR bool operator>(eangle_t const& lhs, eangle_t const& rhs) {
  return lhs.erad() > rhs.erad();
}

EMB_INLINE_CONSTEXPR bool operator<=(eangle_t const& lhs, eangle_t const& rhs) {
  return lhs.erad() <= rhs.erad();
}

EMB_INLINE_CONSTEXPR bool operator>=(eangle_t const& lhs, eangle_t const& rhs) {
  return lhs.erad() >= rhs.erad();
}

EMB_INLINE_CONSTEXPR eangle_t
operator+(eangle_t const& lhs, eangle_t const& rhs) {
  eangle_t tmp = lhs;
  return tmp += rhs;
}

EMB_INLINE_CONSTEXPR eangle_t
operator-(eangle_t const& lhs, eangle_t const& rhs) {
  eangle_t tmp = lhs;
  return tmp -= rhs;
}

EMB_INLINE_CONSTEXPR eangle_t operator*(eangle_t const& lhs, float rhs) {
  return eangle_t(lhs.erad() * rhs);
}

EMB_INLINE_CONSTEXPR eangle_t operator*(float lhs, eangle_t const& rhs) {
  return rhs * lhs;
}

EMB_INLINE_CONSTEXPR eangle_t operator/(eangle_t const& lhs, float rhs) {
  return eangle_t(lhs.erad() / rhs);
}

/*============================================================================*/

/*============================================================================*/
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

  EMB_CONSTEXPR angle_t& operator+=(angle_t const& rhs) {
    rad_ += rhs.rad_;
    return *this;
  }

  EMB_CONSTEXPR angle_t& operator-=(angle_t const& rhs) {
    rad_ -= rhs.rad_;
    return *this;
  }
private:
  EMB_CONSTEXPR void set(rad_t v) { rad_ = v; }

  EMB_CONSTEXPR void set(deg_t v) { rad_ = rad_t(to_rad(v.numval())); }
};

EMB_INLINE_CONSTEXPR bool operator==(angle_t const& lhs, angle_t const& rhs) {
  return lhs.rad() == rhs.rad();
}

EMB_INLINE_CONSTEXPR bool operator!=(angle_t const& lhs, angle_t const& rhs) {
  return lhs.rad() != rhs.rad();
}

EMB_INLINE_CONSTEXPR bool operator<(angle_t const& lhs, angle_t const& rhs) {
  return lhs.rad() < rhs.rad();
}

EMB_INLINE_CONSTEXPR bool operator>(angle_t const& lhs, angle_t const& rhs) {
  return lhs.rad() > rhs.rad();
}

EMB_INLINE_CONSTEXPR bool operator<=(angle_t const& lhs, angle_t const& rhs) {
  return lhs.rad() <= rhs.rad();
}

EMB_INLINE_CONSTEXPR bool operator>=(angle_t const& lhs, angle_t const& rhs) {
  return lhs.rad() >= rhs.rad();
}

EMB_INLINE_CONSTEXPR angle_t operator+(angle_t const& lhs, angle_t const& rhs) {
  angle_t tmp = lhs;
  return tmp += rhs;
}

EMB_INLINE_CONSTEXPR angle_t operator-(angle_t const& lhs, angle_t const& rhs) {
  angle_t tmp = lhs;
  return tmp -= rhs;
}

EMB_INLINE_CONSTEXPR angle_t operator*(angle_t const& lhs, float rhs) {
  return angle_t(lhs.rad() * rhs);
}

EMB_INLINE_CONSTEXPR angle_t operator*(float lhs, angle_t const& rhs) {
  return rhs * lhs;
}

EMB_INLINE_CONSTEXPR angle_t operator/(angle_t const& lhs, float rhs) {
  return angle_t(lhs.rad() / rhs);
}

/*============================================================================*/

} // namespace units
} // namespace emb

template<typename T, typename Unit>
inline emb::units::named_unit<T, Unit>
abs(emb::units::named_unit<T, Unit> const& v) {
  return emb::units::named_unit<T, Unit>(abs(v.numval()));
}
