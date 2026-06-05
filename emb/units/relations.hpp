#pragma once

// cross-domain operators that relate units from different quantity groups.

#include <emb/units/angle.hpp>
#include <emb/units/chrono.hpp>
#include <emb/units/speed.hpp>

#include <concepts>

namespace emb {
namespace units {

// electrical angle / time <-> electrical angular speed

template<std::floating_point T>
constexpr eradps<T> operator/(erad<T> lhs, sec<T> rhs) {
  return eradps<T>(lhs.value() / rhs.value());
}

template<std::floating_point T>
constexpr erad<T> operator*(eradps<T> lhs, sec<T> rhs) {
  return erad<T>(lhs.value() * rhs.value());
}

template<std::floating_point T>
constexpr erad<T> operator*(sec<T> lhs, eradps<T> rhs) {
  return rhs * lhs;
}

} // namespace units
} // namespace emb
