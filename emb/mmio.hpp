#pragma once

#include <emb/meta.hpp>

#include <bit>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace emb {
namespace mmio {

template<typename T>
concept some_register = emb::same_as_any<
    std::remove_cv_t<T>,
    std::uint8_t,
    std::uint16_t,
    std::uint32_t,
    std::uint64_t>;

template<typename T>
concept some_writable_register = some_register<T> && !std::is_const_v<T>;

template<typename T>
using mask_type = std::remove_cv_t<T>;

namespace detail {

template<std::unsigned_integral T>
constexpr bool is_contiguous_mask(T mask) {
  T const x = static_cast<T>(mask >> std::countr_zero(mask));
  return (x & (x + T{1})) == T{0};
}

} // namespace detail

template<auto Mask>
concept valid_mask = std::integral<decltype(Mask)> && (Mask > 0);

template<auto Mask>
concept field_mask = valid_mask<Mask>
                  && detail::is_contiguous_mask(
                         static_cast<std::make_unsigned_t<decltype(Mask)>>(Mask)
                  );

template<auto Mask, typename T>
concept mask_for = some_register<T>
                && valid_mask<Mask>
                && std::in_range<std::remove_cv_t<T>>(Mask);

template<auto Mask, typename T>
concept field_mask_for = mask_for<Mask, T> && field_mask<Mask>;

template<auto Mask, typename T>
concept flag_mask_for = mask_for<Mask, T>
                     && std::has_single_bit(
                            static_cast<std::remove_cv_t<T>>(Mask)
                     );

namespace runtime {

template<some_register T>
[[nodiscard]] auto read(T const& reg, mask_type<T> mask)
    -> std::remove_cv_t<T> {
  using U = std::remove_cv_t<T>;
  return static_cast<U>((reg & mask) >> std::countr_zero(mask));
}

template<some_writable_register T>
void write(T& reg, mask_type<T> mask, mask_type<T> value) {
  using U = std::remove_cv_t<T>;
  reg = static_cast<U>(
      (reg & ~mask) | ((value << std::countr_zero(mask)) & mask)
  );
}

} // namespace runtime

template<auto Mask, some_register T>
  requires field_mask_for<Mask, T>
[[nodiscard]] auto read(T const& reg) -> std::remove_cv_t<T> {
  using U = std::remove_cv_t<T>;
  return runtime::read(reg, static_cast<U>(Mask));
}

template<auto Mask, some_writable_register T>
  requires field_mask_for<Mask, T>
void write(T& reg, mask_type<T> value) {
  using U = std::remove_cv_t<T>;
  runtime::write(reg, static_cast<U>(Mask), value);
}

template<some_writable_register T>
void set(T& reg, mask_type<T> mask) {
  using U = std::remove_cv_t<T>;
  reg = static_cast<U>(reg | mask);
}

template<auto Mask, some_writable_register T>
  requires mask_for<Mask, T>
void set(T& reg) {
  using U = std::remove_cv_t<T>;
  set(reg, static_cast<U>(Mask));
}

template<some_writable_register T>
void clear(T& reg, mask_type<T> mask) {
  using U = std::remove_cv_t<T>;
  reg = static_cast<U>(reg & ~mask);
}

template<auto Mask, some_writable_register T>
  requires mask_for<Mask, T>
void clear(T& reg) {
  using U = std::remove_cv_t<T>;
  clear(reg, static_cast<U>(Mask));
}

template<some_writable_register T>
void toggle(T& reg, mask_type<T> mask) {
  using U = std::remove_cv_t<T>;
  reg = static_cast<U>(reg ^ mask);
}

template<auto Mask, some_writable_register T>
  requires mask_for<Mask, T>
void toggle(T& reg) {
  using U = std::remove_cv_t<T>;
  toggle(reg, static_cast<U>(Mask));
}

template<some_register T>
[[nodiscard]] auto test_any(T const& reg, mask_type<T> mask) -> bool {
  return (reg & mask) != 0;
}

template<auto Mask, some_register T>
  requires mask_for<Mask, T>
[[nodiscard]] auto test_any(T const& reg) -> bool {
  using U = std::remove_cv_t<T>;
  return test_any(reg, static_cast<U>(Mask));
}

template<some_register T>
[[nodiscard]] auto test_all(T const& reg, mask_type<T> mask) -> bool {
  return (reg & mask) == mask;
}

template<auto Mask, some_register T>
  requires mask_for<Mask, T>
[[nodiscard]] auto test_all(T const& reg) -> bool {
  using U = std::remove_cv_t<T>;
  return test_all(reg, static_cast<U>(Mask));
}

template<auto Mask, some_register T>
  requires flag_mask_for<Mask, T>
[[nodiscard]] auto test(T const& reg) -> bool {
  using U = std::remove_cv_t<T>;
  return test_any(reg, static_cast<U>(Mask));
}

template<some_writable_register T>
void clear_w1(T& reg, mask_type<T> mask) {
  using U = std::remove_cv_t<T>;
  reg = static_cast<U>(mask);
}

template<auto Mask, some_writable_register T>
  requires mask_for<Mask, T>
void clear_w1(T& reg) {
  using U = std::remove_cv_t<T>;
  clear_w1(reg, static_cast<U>(Mask));
}

template<some_writable_register T>
void clear_w0(T& reg, mask_type<T> mask) {
  using U = std::remove_cv_t<T>;
  reg = static_cast<U>(~mask);
}

template<auto Mask, some_writable_register T>
  requires mask_for<Mask, T>
void clear_w0(T& reg) {
  using U = std::remove_cv_t<T>;
  clear_w0(reg, static_cast<U>(Mask));
}

template<auto Mask>
  requires field_mask<Mask>
struct bits {
  using value_type = decltype(Mask);
  static constexpr value_type mask = Mask;
  static constexpr unsigned offset = std::countr_zero(mask);

  value_type encoded;

  constexpr explicit bits(value_type v)
      : encoded(value_type((v << offset) & mask)) {}
};

template<some_writable_register T, typename First, typename... Rest>
void modify(T& reg, First first, Rest... rest) {
  using U = std::remove_cv_t<T>;
  static_assert(
      field_mask_for<First::mask, T> && (field_mask_for<Rest::mask, T> && ...),
      "field mask incompatible with this register"
  );
  constexpr auto mask_or = static_cast<U>((First::mask | ... | Rest::mask));
  constexpr auto mask_sum = static_cast<U>(
      (static_cast<U>(First::mask) + ... + static_cast<U>(Rest::mask))
  );

  static_assert(mask_or == mask_sum, "overlapping field masks");

  reg = static_cast<U>((reg & ~mask_or) | (first.encoded | ... | rest.encoded));
}

template<some_writable_register T>
void set_or_clear(T& reg, mask_type<T> mask, bool cond) {
  if (cond)
    set(reg, mask);
  else
    clear(reg, mask);
}

template<auto Mask, some_writable_register T>
  requires mask_for<Mask, T>
void set_or_clear(T& reg, bool cond) {
  if (cond)
    set<Mask>(reg);
  else
    clear<Mask>(reg);
}

} // namespace mmio
} // namespace emb
