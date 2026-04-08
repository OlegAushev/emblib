#pragma once

#include <emb/meta.hpp>

#include <bit>
#include <concepts>
#include <cstdint>
#include <type_traits>

namespace emb {
namespace mmio {

template<typename T>
concept some_register =
    emb::same_as_any<T, uint8_t, uint16_t, uint32_t, uint64_t>;

template<typename T>
using mask_type = std::type_identity_t<T>;

template<some_register T>
[[nodiscard]] auto read(T const volatile& reg, mask_type<T> mask) -> T {
  return static_cast<T>((reg & mask) >> std::countr_zero(mask));
}

template<some_register T>
void write(T volatile& reg, mask_type<T> mask, mask_type<T> value) {
  reg = static_cast<T>(
      (reg & ~mask) | ((value << std::countr_zero(mask)) & mask)
  );
}

template<some_register T>
void set(T volatile& reg, mask_type<T> mask) {
  reg = static_cast<T>(reg | mask);
}

template<some_register T>
void clear(T volatile& reg, mask_type<T> mask) {
  reg = static_cast<T>(reg & ~mask);
}

template<some_register T>
void toggle(T volatile& reg, mask_type<T> mask) {
  reg = static_cast<T>(reg ^ mask);
}

template<some_register T>
[[nodiscard]] auto test_any(T const volatile& reg, mask_type<T> mask) -> bool {
  return (reg & mask) != 0;
}

template<some_register T>
[[nodiscard]] auto test_all(T const volatile& reg, mask_type<T> mask) -> bool {
  return (reg & mask) == mask;
}

template<some_register T>
void clear_w1(T volatile& reg, mask_type<T> mask) {
  reg = static_cast<T>(mask);
}

template<some_register T>
void clear_w0(T volatile& reg, mask_type<T> mask) {
  reg = static_cast<T>(~mask);
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
void modify(T volatile& reg, First first, Rest... rest) {
  constexpr auto mask_or = static_cast<T>(
      (First::mask | ... | Rest::mask)
  ); // binary fold — ok с пустым Rest
  constexpr auto mask_sum = static_cast<T>(
      (static_cast<T>(First::mask) + ... + static_cast<T>(Rest::mask))
  );

  static_assert(mask_or == mask_sum, "overlapping field masks");

  reg = static_cast<T>((reg & ~mask_or) | (first.encoded | ... | rest.encoded));
}

} // namespace mmio
} // namespace emb
