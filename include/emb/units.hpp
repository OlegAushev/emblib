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
  underlying_type v_;
public:
  EMB_CONSTEXPR named_unit() : v_(underlying_type(0)) {}

  EMB_CONSTEXPR explicit named_unit(underlying_type const& v) : v_(v) {}

  EMB_CONSTEXPR underlying_type const& numval() const { return v_; }

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

template<typename T, typename Unit, typename V>
EMB_INLINE_CONSTEXPR named_unit<T, Unit>
operator*(named_unit<T, Unit> const& lhs, V const& rhs) {
  return named_unit<T, Unit>(
      lhs.numval() * static_cast<named_unit<T, Unit>::underlying_type>(rhs));
}

template<typename T, typename Unit, typename V>
EMB_INLINE_CONSTEXPR named_unit<T, Unit>
operator*(V const& lhs, named_unit<T, Unit> const& rhs) {
  return rhs * lhs;
}

template<typename T, typename Unit, typename V>
EMB_INLINE_CONSTEXPR named_unit<T, Unit>
operator/(named_unit<T, Unit> const& lhs, V const& rhs) {
  return named_unit<T, Unit>(
      lhs.numval() / static_cast<named_unit<T, Unit>::underlying_type>(rhs));
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR named_unit<T, Unit>
operator-(named_unit<T, Unit> const& v) {
  return named_unit<T, Unit>(-v.numval());
}

/*============================================================================*/

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

/*============================================================================*/

#if defined (__cpp_variadic_templates) && defined (__cpp_concepts)

template<typename To, typename From, typename... Args>
  requires(!std::same_as<To, From>)
EMB_INLINE_CONSTEXPR To convert_to(From const& v, Args... args);

template<typename To, typename From, typename... Args>
  requires std::same_as<To, From>
EMB_INLINE_CONSTEXPR To convert_to(From const& v, Args... args) {
  return v;
}

#else

template<typename To, typename From>
EMB_INLINE_CONSTEXPR To convert_to(From const& v);

template<typename To, typename From, typename Arg>
EMB_INLINE_CONSTEXPR To convert_to(From const& v, Arg arg);

#endif

template<>
EMB_INLINE_CONSTEXPR erad_f32 convert_to(edeg_f32 const& v) {
  return units::erad_f32(emb::to_rad(v.numval()));
}

template<>
EMB_INLINE_CONSTEXPR edeg_f32 convert_to(erad_f32 const& v) {
  return units::edeg_f32(emb::to_deg(v.numval()));
}

template<>
EMB_INLINE_CONSTEXPR rad_f32 convert_to(deg_f32 const& v) {
  return units::rad_f32(emb::to_rad(v.numval()));
}

template<>
EMB_INLINE_CONSTEXPR deg_f32 convert_to(rad_f32 const& v) {
  return units::deg_f32(emb::to_deg(v.numval()));
}

#if defined (__cpp_variadic_templates) && defined (__cpp_concepts)

template<>
EMB_INLINE_CONSTEXPR eradps_f32 convert_to(rpm_f32 const& v, int32_t p) {
  return units::eradps_f32(emb::to_eradps(v.numval(), p));
}

template<>
EMB_INLINE_CONSTEXPR rpm_f32 convert_to(eradps_f32 const& v, int32_t p) {
  return units::rpm_f32(emb::to_rpm(v.numval(), p));
}

#else

template<>
EMB_INLINE_CONSTEXPR eradps_f32 convert_to(rpm_f32 const& v, int32_t p) {
  return units::eradps_f32(emb::to_eradps(v.numval(), p));
}

template<>
EMB_INLINE_CONSTEXPR rpm_f32 convert_to(eradps_f32 const& v, int32_t p) {
  return units::rpm_f32(emb::to_rpm(v.numval(), p));
}

#endif

/*============================================================================*/

// class motorspeed_f32 {
// public:
//   typedef float underlying_type;
// private:
//   int32_t p_;
//   eradps_f32 w_;
// public:
//   EMB_CONSTEXPR explicit motorspeed_f32(int32_t p) : p_(p), w_(0) {}

//   EMB_CONSTEXPR motorspeed_f32(int32_t p, eradps_f32 w) : p_(p) { set(w); }

//   EMB_CONSTEXPR motorspeed_f32(int32_t p, rpm_f32 n) : p_(p) { set(n); }

//   template<typename Unit>
//   EMB_CONSTEXPR motorspeed_f32& operator=(Unit v) {
//     set(v);
//     return *this;
//   }

//   EMB_CONSTEXPR int32_t p() const { return p_; }

//   EMB_CONSTEXPR eradps_f32 eradps() const { return w_; }

//   EMB_CONSTEXPR rpm_f32 rpm() const { return convert_to<rpm_f32>(w_, p_); }
// private:
//   EMB_CONSTEXPR void set(eradps_f32 w) { w_ = w; }

//   EMB_CONSTEXPR void set(rpm_f32 n) { w_ = convert_to<eradps_f32>(n, p_); }
// };

// // NOTE: if lhs.p() != rhs.p() then comparison operator behaviour is undefined
// EMB_INLINE_CONSTEXPR bool
// operator==(motorspeed_f32 const& lhs, motorspeed_f32 const& rhs) {
//   assert(lhs.p() == rhs.p());
//   return lhs.eradps() == rhs.eradps();
// }

// EMB_INLINE_CONSTEXPR bool
// operator!=(motorspeed_f32 const& lhs, motorspeed_f32 const& rhs) {
//   assert(lhs.p() == rhs.p());
//   return lhs.eradps() != rhs.eradps();
// }

// EMB_INLINE_CONSTEXPR bool
// operator<(motorspeed_f32 const& lhs, motorspeed_f32 const& rhs) {
//   assert(lhs.p() == rhs.p());
//   return lhs.eradps() < rhs.eradps();
// }

// EMB_INLINE_CONSTEXPR bool
// operator>(motorspeed_f32 const& lhs, motorspeed_f32 const& rhs) {
//   assert(lhs.p() == rhs.p());
//   return lhs.eradps() > rhs.eradps();
// }

// EMB_INLINE_CONSTEXPR bool
// operator<=(motorspeed_f32 const& lhs, motorspeed_f32 const& rhs) {
//   assert(lhs.p() == rhs.p());
//   return lhs.eradps() <= rhs.eradps();
// }

// EMB_INLINE_CONSTEXPR bool
// operator>=(motorspeed_f32 const& lhs, motorspeed_f32 const& rhs) {
//   assert(lhs.p() == rhs.p());
//   return lhs.eradps() >= rhs.eradps();
// }

// template<typename V>
// EMB_INLINE_CONSTEXPR motorspeed_f32
// operator*(motorspeed_f32 const& lhs, V const& rhs) {
//   return motorspeed_f32(lhs.p(), lhs.eradps() * rhs);
// }

// template<typename V>
// EMB_INLINE_CONSTEXPR motorspeed_f32
// operator*(V const& lhs, motorspeed_f32 const& rhs) {
//   return rhs * lhs;
// }

// template<typename V>
// EMB_INLINE_CONSTEXPR motorspeed_f32
// operator/(motorspeed_f32 const& lhs, V const& rhs) {
//   return motorspeed_f32(lhs.p(), lhs.eradps() / rhs);
// }

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

} // namespace emb

template<typename T, typename Unit>
inline emb::units::named_unit<T, Unit>
abs(emb::units::named_unit<T, Unit> const& v) {
  return emb::units::named_unit<T, Unit>(abs(v.numval()));
}
