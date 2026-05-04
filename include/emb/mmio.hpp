#pragma once

#include <emb/meta.hpp>

#include <bit>
#include <concepts>
#include <cstdint>
#include <type_traits>

namespace emb {
namespace mmio {

template<typename T>
concept some_register = emb::
    same_as_any<std::remove_cv_t<T>, uint8_t, uint16_t, uint32_t, uint64_t>;

template<typename T>
using mask_type = std::type_identity_t<std::remove_cv_t<T>>;

template<some_register T>
[[nodiscard]] auto read(T const& reg, mask_type<T> mask)
    -> std::remove_cv_t<T> {
  using U = std::remove_cv_t<T>;
  return static_cast<U>((reg & mask) >> std::countr_zero(mask));
}

template<some_register T>
  requires(!std::is_const_v<T>)
void write(T& reg, mask_type<T> mask, mask_type<T> value) {
  using U = std::remove_cv_t<T>;
  reg = static_cast<U>(
      (reg & ~mask) | ((value << std::countr_zero(mask)) & mask)
  );
}

template<some_register T>
  requires(!std::is_const_v<T>)
void set(T& reg, mask_type<T> mask) {
  using U = std::remove_cv_t<T>;
  reg = static_cast<U>(reg | mask);
}

template<some_register T>
  requires(!std::is_const_v<T>)
void clear(T& reg, mask_type<T> mask) {
  using U = std::remove_cv_t<T>;
  reg = static_cast<U>(reg & ~mask);
}

template<some_register T>
  requires(!std::is_const_v<T>)
void toggle(T& reg, mask_type<T> mask) {
  using U = std::remove_cv_t<T>;
  reg = static_cast<U>(reg ^ mask);
}

template<some_register T>
[[nodiscard]] auto test_any(T const& reg, mask_type<T> mask) -> bool {
  return (reg & mask) != 0;
}

template<some_register T>
[[nodiscard]] auto test_all(T const& reg, mask_type<T> mask) -> bool {
  return (reg & mask) == mask;
}

template<some_register T>
  requires(!std::is_const_v<T>)
void clear_w1(T& reg, mask_type<T> mask) {
  using U = std::remove_cv_t<T>;
  reg = static_cast<U>(mask);
}

template<some_register T>
  requires(!std::is_const_v<T>)
void clear_w0(T& reg, mask_type<T> mask) {
  using U = std::remove_cv_t<T>;
  reg = static_cast<U>(~mask);
}

template<auto Mask>
  requires(Mask != 0)
struct bits {
  using value_type = decltype(Mask);
  static constexpr value_type mask = Mask;
  static constexpr unsigned offset = std::countr_zero(mask);

  value_type encoded;

  constexpr explicit bits(value_type v)
      : encoded(value_type((v << offset) & mask)) {}
};

template<some_register T, typename First, typename... Rest>
  requires(!std::is_const_v<T>)
void modify(T& reg, First first, Rest... rest) {
  using U = std::remove_cv_t<T>;
  constexpr auto mask_or = static_cast<U>((First::mask | ... | Rest::mask));
  constexpr auto mask_sum = static_cast<U>(
      (static_cast<U>(First::mask) + ... + static_cast<U>(Rest::mask))
  );

  static_assert(mask_or == mask_sum, "overlapping field masks");

  reg = static_cast<U>((reg & ~mask_or) | (first.encoded | ... | rest.encoded));
}

template<some_register T>
  requires(!std::is_const_v<T>)
void set_or_clear(T& reg, mask_type<T> mask, bool cond) {
  if (cond) set(reg, mask);
  else      clear(reg, mask);
}

} // namespace mmio
} // namespace emb
