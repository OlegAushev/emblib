#pragma once

#include <concepts>
#include <type_traits>

namespace emb {
namespace units {

template<std::floating_point T, typename Unit>
class named_unit {
public:
  using value_type = T;
  using unit_type = Unit;
public:
  value_type v_;
public:
  constexpr named_unit() : v_(value_type{0}) {}

  constexpr explicit named_unit(value_type v) : v_(v) {}

  constexpr value_type value() const {
    return v_;
  }

  constexpr named_unit& operator+=(named_unit rhs) {
    v_ += rhs.v_;
    return *this;
  }

  constexpr named_unit& operator-=(named_unit rhs) {
    v_ -= rhs.v_;
    return *this;
  }
};

template<std::floating_point T, typename Unit>
constexpr bool
operator==(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs) {
  return lhs.value() == rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool
operator!=(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs) {
  return lhs.value() != rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool
operator<(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs) {
  return lhs.value() < rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool
operator>(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs) {
  return lhs.value() > rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool
operator<=(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs) {
  return lhs.value() <= rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool
operator>=(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs) {
  return lhs.value() >= rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr named_unit<T, Unit>
operator+(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs) {
  return lhs += rhs;
}

template<std::floating_point T, typename Unit>
constexpr named_unit<T, Unit>
operator-(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs) {
  return lhs -= rhs;
}

template<std::floating_point T, typename Unit, typename V>
  requires std::is_arithmetic_v<V>
constexpr named_unit<T, Unit> operator*(named_unit<T, Unit> lhs, V rhs) {
  return named_unit<T, Unit>(
      lhs.value() * static_cast<typename named_unit<T, Unit>::value_type>(rhs)
  );
}

template<std::floating_point T, typename Unit, typename V>
  requires std::is_arithmetic_v<V>
constexpr named_unit<T, Unit> operator*(V lhs, named_unit<T, Unit> rhs) {
  return rhs * lhs;
}

template<std::floating_point T, typename Unit, typename V>
  requires std::is_arithmetic_v<V>
constexpr named_unit<T, Unit> operator/(named_unit<T, Unit> lhs, V rhs) {
  return named_unit<T, Unit>(
      lhs.value() / static_cast<typename named_unit<T, Unit>::value_type>(rhs)
  );
}

template<std::floating_point T, typename Unit>
constexpr T
operator/(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs) {
  return lhs.value() / rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr named_unit<T, Unit> operator-(named_unit<T, Unit> v) {
  return named_unit<T, Unit>(-v.value());
}

template<typename T>
concept unit =
    std::same_as<T, named_unit<typename T::value_type, typename T::unit_type>>;

template<typename To, typename From, typename... Args>
  requires std::same_as<To, From>
constexpr To convert_to(From v, Args... args) {
  return v;
}

} // namespace units
} // namespace emb
