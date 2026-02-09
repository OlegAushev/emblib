#pragma once

#include <emb/math.hpp>

#include <concepts>

namespace emb {
namespace units {

template<std::floating_point T, typename Unit>
class named_unit {
public:
  using value_type = T;
  using unit_type = Unit;
private:
  value_type v_;
public:
  constexpr named_unit() : v_(value_type{0}) {}

  constexpr explicit named_unit(value_type const& v) : v_(v) {}

  constexpr value_type const& value() const {
    return v_;
  }

  constexpr named_unit& operator+=(named_unit const& rhs) {
    v_ += rhs.v_;
    return *this;
  }

  constexpr named_unit& operator-=(named_unit const& rhs) {
    v_ -= rhs.v_;
    return *this;
  }
};

template<std::floating_point T, typename Unit>
constexpr bool
operator==(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  return lhs.value() == rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool
operator!=(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  return lhs.value() != rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool
operator<(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  return lhs.value() < rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool
operator>(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  return lhs.value() > rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool
operator<=(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  return lhs.value() <= rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool
operator>=(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  return lhs.value() >= rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr named_unit<T, Unit>
operator+(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  named_unit<T, Unit> tmp = lhs;
  return tmp += rhs;
}

template<std::floating_point T, typename Unit>
constexpr named_unit<T, Unit>
operator-(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  named_unit<T, Unit> tmp = lhs;
  return tmp -= rhs;
}

template<std::floating_point T, typename Unit, typename V>
  requires std::is_arithmetic_v<V>
constexpr named_unit<T, Unit> operator*(named_unit<T, Unit> const& lhs, V rhs) {
  return named_unit<T, Unit>(
      lhs.value() * static_cast<typename named_unit<T, Unit>::value_type>(rhs)
  );
}

template<std::floating_point T, typename Unit, typename V>
  requires std::is_arithmetic_v<V>
constexpr named_unit<T, Unit> operator*(V lhs, named_unit<T, Unit> const& rhs) {
  return rhs * lhs;
}

template<std::floating_point T, typename Unit, typename V>
  requires std::is_arithmetic_v<V>
constexpr named_unit<T, Unit> operator/(named_unit<T, Unit> const& lhs, V rhs) {
  return named_unit<T, Unit>(
      lhs.value() / static_cast<typename named_unit<T, Unit>::value_type>(rhs)
  );
}

template<std::floating_point T, typename Unit>
constexpr T
operator/(named_unit<T, Unit> const& lhs, named_unit<T, Unit> const& rhs) {
  return lhs.value() / rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr named_unit<T, Unit> operator-(named_unit<T, Unit> const& v) {
  return named_unit<T, Unit>(-v.value());
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

struct sec {};

} // namespace tags

template<std::floating_point T>
using rpm = named_unit<T, tags::rpm>;

template<std::floating_point T>
using eradps = named_unit<T, tags::eradps>;

template<std::floating_point T>
using erad = named_unit<T, tags::erad>;

template<std::floating_point T>
using edeg = named_unit<T, tags::edeg>;

template<std::floating_point T>
using rad = named_unit<T, tags::rad>;

template<std::floating_point T>
using deg = named_unit<T, tags::deg>;

template<std::floating_point T>
using hz = named_unit<T, tags::hz>;

template<std::floating_point T>
using sec = named_unit<T, tags::sec>;

using rpm_f32 = rpm<float>;
using eradps_f32 = eradps<float>;
using erad_f32 = erad<float>;
using edeg_f32 = edeg<float>;
using rad_f32 = rad<float>;
using deg_f32 = deg<float>;
using hz_f32 = hz<float>;
using sec_f32 = sec<float>;

template<typename T>
concept unit =
    std::same_as<T, named_unit<typename T::value_type, typename T::unit_type>>;

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

// template<typename To, typename From, typename... Args>
//   requires(!std::same_as<To, From>)
// constexpr To convert_to(From const& v, Args... args);

template<typename To, typename From, typename... Args>
  requires std::same_as<To, From>
constexpr To convert_to(From const& v, Args... args) {
  return v;
}

template<typename To, std::floating_point T>
  requires std::same_as<To, erad<T>>
constexpr erad<T> convert_to(edeg<T> const& v) {
  return units::erad<T>(emb::to_rad(v.value()));
}

template<typename To, std::floating_point T>
  requires std::same_as<To, edeg<T>>
constexpr edeg<T> convert_to(erad<T> const& v) {
  return units::edeg<T>(emb::to_deg(v.value()));
}

template<typename To, std::floating_point T>
  requires std::same_as<To, rad<T>>
constexpr rad<T> convert_to(deg<T> const& v) {
  return units::rad<T>(emb::to_rad(v.value()));
}

template<typename To, std::floating_point T>
  requires std::same_as<To, deg<T>>
constexpr deg<T> convert_to(rad<T> const& v) {
  return units::deg<T>(emb::to_deg(v.value()));
}

template<typename To, std::floating_point T, std::integral P>
  requires std::same_as<To, eradps<T>>
constexpr eradps<T> convert_to(rpm<T> const& v, P p) {
  return units::eradps<T>(emb::to_eradps(v.value(), p));
}

template<typename To, std::floating_point T, std::integral P>
  requires std::same_as<To, rpm<T>>
constexpr rpm<T> convert_to(eradps<T> const& v, P p) {
  return units::rpm<T>(emb::to_rpm(v.value(), p));
}

template<std::floating_point T>
constexpr eradps<T> operator/(erad<T> const& lhs, sec<T> const& rhs) {
  return eradps<T>(lhs.value() / rhs.value());
}

template<std::floating_point T>
constexpr erad<T> operator*(eradps<T> const& lhs, sec<T> const& rhs) {
  return erad<T>(lhs.value() * rhs.value());
}

template<std::floating_point T>
constexpr erad<T> operator*(sec<T> const& lhs, eradps<T> const& rhs) {
  return rhs * lhs;
}

template<std::floating_point T, typename V>
  requires std::is_arithmetic_v<V>
constexpr sec<T> operator/(V lhs, hz<T> const& rhs) {
  return sec<T>(static_cast<T>(lhs) / rhs.value());
}

template<std::floating_point T, typename V>
  requires std::is_arithmetic_v<V>
constexpr hz<T> operator/(V lhs, sec<T> const& rhs) {
  return hz<T>(static_cast<T>(lhs) / rhs.value());
}

} // namespace units

template<units::unit_of_radians Unit>
constexpr Unit rem2pi(Unit v) {
  return Unit{rem2pi(v.value())};
}

template<units::unit_of_radians Unit>
constexpr Unit rempi(Unit v) {
  return Unit{rempi(v.value())};
}

template<units::unit_of_degrees Unit>
constexpr Unit rem360(Unit v) {
  using value_type = Unit::value_type;
  value_type v_ = emb::fmod(
      v.value(),
      to_deg(2 * std::numbers::pi_v<value_type>)
  );
  if (v_ < 0) {
    v_ += to_deg(2 * std::numbers::pi_v<value_type>);
  }
  return Unit{v_};
}

template<units::unit_of_degrees Unit>
constexpr Unit rem180(Unit v) {
  using value_type = Unit::value_type;
  value_type v_ = emb::fmod(
      v.value() + to_deg(std::numbers::pi_v<value_type>),
      to_deg(2 * std::numbers::pi_v<value_type>)
  );
  if (v_ < 0) {
    v_ += to_deg(2 * std::numbers::pi_v<value_type>);
  }
  return Unit{v_ - to_deg(std::numbers::pi_v<value_type>)};
}

} // namespace emb
