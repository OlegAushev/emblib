#pragma once

#include <emb/core.hpp>
#include <emb/math.hpp>

namespace emb {
namespace units {

template<typename T, typename Unit>
class named_unit {
public:
  typedef T underlying_type;
  typedef Unit unit_type;
private:
  underlying_type v_;
public:
  EMB_CONSTEXPR named_unit() : v_(underlying_type(0)) {}

  EMB_CONSTEXPR explicit named_unit(underlying_type const& v) : v_(v) {}

  EMB_CONSTEXPR underlying_type const& numval() const {
    return v_;
  }

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
EMB_INLINE_CONSTEXPR bool operator==(
    named_unit<T, Unit> const& lhs,
    named_unit<T, Unit> const& rhs
) {
  return lhs.numval() == rhs.numval();
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR bool operator!=(
    named_unit<T, Unit> const& lhs,
    named_unit<T, Unit> const& rhs
) {
  return lhs.numval() != rhs.numval();
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR bool operator<(
    named_unit<T, Unit> const& lhs,
    named_unit<T, Unit> const& rhs
) {
  return lhs.numval() < rhs.numval();
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR bool operator>(
    named_unit<T, Unit> const& lhs,
    named_unit<T, Unit> const& rhs
) {
  return lhs.numval() > rhs.numval();
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR bool operator<=(
    named_unit<T, Unit> const& lhs,
    named_unit<T, Unit> const& rhs
) {
  return lhs.numval() <= rhs.numval();
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR bool operator>=(
    named_unit<T, Unit> const& lhs,
    named_unit<T, Unit> const& rhs
) {
  return lhs.numval() >= rhs.numval();
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR named_unit<T, Unit> operator+(
    named_unit<T, Unit> const& lhs,
    named_unit<T, Unit> const& rhs
) {
  named_unit<T, Unit> tmp = lhs;
  return tmp += rhs;
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR named_unit<T, Unit> operator-(
    named_unit<T, Unit> const& lhs,
    named_unit<T, Unit> const& rhs
) {
  named_unit<T, Unit> tmp = lhs;
  return tmp -= rhs;
}

template<typename T, typename Unit, typename V>
EMB_INLINE_CONSTEXPR named_unit<T, Unit> operator*(
    named_unit<T, Unit> const& lhs,
    V const& rhs
) {
  return named_unit<T, Unit>(
      lhs.numval() *
      static_cast<typename named_unit<T, Unit>::underlying_type>(rhs)
  );
}

template<typename T, typename Unit, typename V>
EMB_INLINE_CONSTEXPR named_unit<T, Unit> operator*(
    V const& lhs,
    named_unit<T, Unit> const& rhs
) {
  return rhs * lhs;
}

template<typename T, typename Unit, typename V>
EMB_INLINE_CONSTEXPR named_unit<T, Unit> operator/(
    named_unit<T, Unit> const& lhs,
    V const& rhs
) {
  return named_unit<T, Unit>(
      lhs.numval() /
      static_cast<typename named_unit<T, Unit>::underlying_type>(rhs)
  );
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR T
operator/(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  return lhs.numval() / rhs.numval();
}

template<typename T, typename Unit>
EMB_INLINE_CONSTEXPR named_unit<T, Unit> operator-(
    named_unit<T, Unit> const& v
) {
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

// chrono
struct hz {};

} // namespace tags

#ifdef __cpp_alias_templates

template<typename T>
using rpm = named_unit<T, tags::rpm>;

template<typename T>
using eradps = named_unit<T, tags::eradps>;

template<typename T>
using erad = named_unit<T, tags::erad>;

template<typename T>
using edeg = named_unit<T, tags::edeg>;

template<typename T>
using rad = named_unit<T, tags::rad>;

template<typename T>
using deg = named_unit<T, tags::deg>;

template<typename T>
using hz = named_unit<T, tags::hz>;

using rpm_f32 = rpm<float>;
using eradps_f32 = eradps<float>;
using erad_f32 = erad<float>;
using edeg_f32 = edeg<float>;
using rad_f32 = rad<float>;
using deg_f32 = deg<float>;
using hz_f32 = hz<float>;

#else

typedef named_unit<float, tags::rpm> rpm_f32;
typedef named_unit<float, tags::eradps> eradps_f32;
typedef named_unit<float, tags::erad> erad_f32;
typedef named_unit<float, tags::edeg> edeg_f32;
typedef named_unit<float, tags::rad> rad_f32;
typedef named_unit<float, tags::deg> deg_f32;

#endif

#ifdef __cpp_concepts

template<typename T>
concept unit = std::same_as<
    T,
    named_unit<typename T::underlying_type, typename T::unit_type>>;

template<typename T>
concept unit_of_rotational_speed =
    unit<T> && (std::same_as<typename T::unit_type, tags::rpm> ||
                std::same_as<typename T::unit_type, tags::eradps>);

template<typename T>
concept unit_of_electrical_angle =
    unit<T> && (std::same_as<typename T::unit_type, tags::erad> ||
                std::same_as<typename T::unit_type, tags::edeg>);

template<typename T>
concept unit_of_angle = unit<T> &&
                        (std::same_as<typename T::unit_type, tags::rad> ||
                         std::same_as<typename T::unit_type, tags::deg>);

template<typename T>
concept unit_of_radians = unit<T> &&
                          (std::same_as<typename T::unit_type, tags::erad> ||
                           std::same_as<typename T::unit_type, tags::rad>);

template<typename T>
concept unit_of_degrees = unit<T> &&
                          (std::same_as<typename T::unit_type, tags::edeg> ||
                           std::same_as<typename T::unit_type, tags::deg>);
#endif

#if defined(__cpp_variadic_templates) && defined(__cpp_concepts)

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

template<>
EMB_INLINE_CONSTEXPR eradps_f32 convert_to(rpm_f32 const& v, int32_t p) {
  return units::eradps_f32(emb::to_eradps(v.numval(), p));
}

template<>
EMB_INLINE_CONSTEXPR rpm_f32 convert_to(eradps_f32 const& v, int32_t p) {
  return units::rpm_f32(emb::to_rpm(v.numval(), p));
}

} // namespace units

#ifdef __cpp_concepts

template<units::unit_of_radians Unit>
constexpr Unit rem2pi(Unit v) {
  return Unit{rem2pi(v.numval())};
}

template<units::unit_of_radians Unit>
constexpr Unit rempi(Unit v) {
  return Unit{rempi(v.numval())};
}

template<units::unit_of_degrees Unit>
constexpr Unit rem360(Unit v) {
  float v_ = emb::fmod(v.numval(), to_deg(2 * numbers::pi));
  if (v_ < 0) {
    v_ += to_deg(2 * numbers::pi);
  }
  return Unit{v_};
}

template<units::unit_of_degrees Unit>
constexpr Unit rem180(Unit v) {
  float v_ = emb::fmod(
      v.numval() + to_deg(numbers::pi),
      to_deg(2 * numbers::pi)
  );
  if (v_ < 0) {
    v_ += to_deg(2 * numbers::pi);
  }
  return Unit{v_ - to_deg(numbers::pi)};
}

#endif

} // namespace emb

template<typename T, typename Unit>
inline emb::units::named_unit<T, Unit> abs(
    emb::units::named_unit<T, Unit> const& v
) {
  return emb::units::named_unit<T, Unit>(std::abs(v.numval()));
}
