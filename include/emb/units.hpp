#pragma once

#include <emb/core.hpp>
#include <emb/math.hpp>

namespace emb {
namespace units {

template<typename T, typename Unit>
class named_unit {
public:
  typedef T underlying_type;
private:
  T v_;
public:
  EMB_CONSTEXPR named_unit() : v_(T(0)) {}

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
EMB_INLINE_CONSTEXPR named_unit<T, Unit> operator*(
    named_unit<T, Unit> const& lhs,
    typename named_unit<T, Unit>::underlying_type rhs) {
  return named_unit<T, Unit>(lhs.numval() * rhs);
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR named_unit<T, Unit> operator*(
    typename named_unit<T, Unit>::underlying_type lhs,
    named_unit<T, Unit> const& rhs) {
  return rhs * lhs;
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR named_unit<T, Unit> operator/(
    named_unit<T, Unit> const& lhs,
    typename named_unit<T, Unit>::underlying_type rhs) {
  return named_unit<T, Unit>(lhs.numval() / rhs);
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR named_unit<T, Unit>
operator-(named_unit<T, Unit> const& v) {
  return named_unit<T, Unit>(-v.numval());
}

namespace tags {
// speed
struct rpm {};

struct eradps {};

// angle
struct erad {};

struct edeg {};

struct rad {};

struct deg {};
} // namespace tags

typedef named_unit<float, tags::rpm> rpm_f32;
typedef named_unit<float, tags::eradps> eradps_f32;

typedef named_unit<float, tags::erad> erad_f32;
typedef named_unit<float, tags::edeg> edeg_f32;
typedef named_unit<float, tags::rad> rad_f32;
typedef named_unit<float, tags::deg> deg_f32;

#ifdef __cpp_concepts

template<typename Unit>
concept RadianUnit =
    std::same_as<Unit, erad_f32> || std::same_as<Unit, rad_f32>;

template<typename Unit>
concept DegreeUnit =
    std::same_as<Unit, edeg_f32> || std::same_as<Unit, deg_f32>;

template<typename Unit>
concept EAngleUnit =
    std::same_as<Unit, erad_f32> || std::same_as<Unit, edeg_f32>;

template<typename Unit>
concept AngleUnit =
    std::same_as<Unit, rad_f32> || std::same_as<Unit, deg_f32>;

#endif

} // namespace units

EMB_INLINE_CONSTEXPR float to_rad(float deg) {
  return numbers::pi * deg / 180.0f;
}

EMB_INLINE_CONSTEXPR float to_deg(float rad) {
  return 180.0f * rad / numbers::pi;
}

EMB_INLINE_CONSTEXPR float to_eradps(float n, int32_t p) {
  return 2 * numbers::pi * float(p) * n / 60.0f;
}

EMB_INLINE_CONSTEXPR float to_rpm(float w, int32_t p) {
  return 60.f * w / (2 * numbers::pi * float(p));
}

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
  float v_ = fmodf(v.numval(), to_deg(2 * numbers::pi));
  if (v_ < 0) {
    v_ += to_deg(2 * numbers::pi);
  }
  return Unit{v_};
}

template<units::DegreeUnit Unit>
Unit rempi(Unit v) {
  float v_ = fmodf(v.numval() + to_deg(numbers::pi), to_deg(2 * numbers::pi));
  if (v_ < 0) {
    v_ += to_deg(2 * numbers::pi);
  }
  return Unit{v_ - to_deg(numbers::pi)};
}

#endif

EMB_INLINE_CONSTEXPR units::erad_f32 to_rad(units::edeg_f32 deg) {
  return units::erad_f32(emb::to_rad(deg.numval()));
}

EMB_INLINE_CONSTEXPR units::edeg_f32 to_deg(units::erad_f32 rad) {
  return units::edeg_f32(emb::to_deg(rad.numval()));
}

EMB_INLINE_CONSTEXPR units::rad_f32 to_rad(units::deg_f32 deg) {
  return units::rad_f32(emb::to_rad(deg.numval()));
}

EMB_INLINE_CONSTEXPR units::deg_f32 to_deg(units::rad_f32 rad) {
  return units::deg_f32(emb::to_deg(rad.numval()));
}

EMB_INLINE_CONSTEXPR units::eradps_f32 to_eradps(units::rpm_f32 n, int32_t p) {
  return units::eradps_f32(emb::to_eradps(n.numval(), p));
}

EMB_INLINE_CONSTEXPR units::rpm_f32 to_rpm(units::eradps_f32 w, int32_t p) {
  return units::rpm_f32(emb::to_rpm(w.numval(), p));
}

namespace units {

/*============================================================================*/
class motorspeed_t {
private:
  int32_t p_;
  eradps_f32 w_;
public:
  EMB_CONSTEXPR explicit motorspeed_t(int32_t p) : p_(p), w_(0) {}

  EMB_CONSTEXPR motorspeed_t(int32_t p, eradps_f32 w) : p_(p) { set(w); }

  EMB_CONSTEXPR motorspeed_t(int32_t p, rpm_f32 n) : p_(p) { set(n); }

  template<typename Unit>
  EMB_CONSTEXPR motorspeed_t& operator=(Unit v) {
    set(v);
    return *this;
  }

  EMB_CONSTEXPR int32_t p() const { return p_; }

  EMB_CONSTEXPR eradps_f32 eradps() const { return w_; }

  EMB_CONSTEXPR rpm_f32 rpm() const { return to_rpm(w_, p_); }
private:
  EMB_CONSTEXPR void set(eradps_f32 w) { w_ = w; }

  EMB_CONSTEXPR void set(rpm_f32 n) { w_ = to_eradps(n, p_); }
};

// NOTE: if lhs.p() != rhs.p() then comparison operator behaviour is undefined
EMB_INLINE_CONSTEXPR bool
operator==(motorspeed_t const& lhs, motorspeed_t const& rhs) {
  assert(lhs.p() == rhs.p());
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

} // namespace units
} // namespace emb

template<typename T, typename Unit>
inline emb::units::named_unit<T, Unit>
abs(emb::units::named_unit<T, Unit> const& v) {
  return emb::units::named_unit<T, Unit>(abs(v.numval()));
}
