#pragma once

#include <emb/meta.hpp>

#include <bit>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace emb {
namespace mmio {

template<typename Reg>
concept some_register = emb::same_as_any<
    std::remove_cv_t<Reg>,
    std::uint8_t,
    std::uint16_t,
    std::uint32_t,
    std::uint64_t>;

template<typename Reg>
concept some_writable_register = some_register<Reg> && !std::is_const_v<Reg>;

template<typename Reg>
using mask_type = std::remove_cv_t<Reg>;

template<typename Reg>
using value_type = std::remove_cv_t<Reg>;

namespace detail {

template<std::unsigned_integral M>
constexpr bool is_contiguous_mask(M mask) {
  M const x = static_cast<M>(mask >> std::countr_zero(mask));
  return (x & (x + M{1})) == M{0};
}

} // namespace detail

template<auto Mask>
concept valid_mask = std::integral<decltype(Mask)> && (Mask > 0);

template<auto Mask>
concept field_mask = valid_mask<Mask>
                  && detail::is_contiguous_mask(
                         static_cast<std::make_unsigned_t<decltype(Mask)>>(Mask)
                  );

template<auto Mask, typename Reg>
concept mask_for = some_register<Reg>
                && valid_mask<Mask>
                && std::in_range<std::remove_cv_t<Reg>>(Mask);

template<auto Mask, typename Reg>
concept field_mask_for = mask_for<Mask, Reg> && field_mask<Mask>;

template<auto Mask, typename Reg>
concept flag_mask_for = mask_for<Mask, Reg>
                     && std::has_single_bit(
                            static_cast<std::remove_cv_t<Reg>>(Mask)
                     );

namespace runtime {

template<some_register Reg>
[[nodiscard]] auto read(Reg const& reg, mask_type<Reg> mask)
    -> value_type<Reg> {
  using U = std::remove_cv_t<Reg>;
  return static_cast<U>((reg & mask) >> std::countr_zero(mask));
}

template<some_writable_register Reg>
void write(Reg& reg, mask_type<Reg> mask, value_type<Reg> value) {
  using U = std::remove_cv_t<Reg>;
  reg = static_cast<U>(
      (reg & ~mask) | ((value << std::countr_zero(mask)) & mask)
  );
}

template<some_writable_register Reg>
void set(Reg& reg, mask_type<Reg> mask) {
  using U = std::remove_cv_t<Reg>;
  reg = static_cast<U>(reg | mask);
}

template<some_writable_register Reg>
void clear(Reg& reg, mask_type<Reg> mask) {
  using U = std::remove_cv_t<Reg>;
  reg = static_cast<U>(reg & ~mask);
}

template<some_writable_register Reg>
void toggle(Reg& reg, mask_type<Reg> mask) {
  using U = std::remove_cv_t<Reg>;
  reg = static_cast<U>(reg ^ mask);
}

template<some_register Reg>
[[nodiscard]] auto test_any(Reg const& reg, mask_type<Reg> mask) -> bool {
  return (reg & mask) != 0;
}

template<some_register Reg>
[[nodiscard]] auto test_all(Reg const& reg, mask_type<Reg> mask) -> bool {
  return (reg & mask) == mask;
}

template<some_writable_register Reg>
void clear_w1(Reg& reg, mask_type<Reg> mask) {
  using U = std::remove_cv_t<Reg>;
  reg = static_cast<U>(mask);
}

template<some_writable_register Reg>
void clear_w0(Reg& reg, mask_type<Reg> mask) {
  using U = std::remove_cv_t<Reg>;
  reg = static_cast<U>(~mask);
}

template<some_writable_register Reg>
void set_or_clear(Reg& reg, mask_type<Reg> mask, bool cond) {
  if (cond)
    set(reg, mask);
  else
    clear(reg, mask);
}

} // namespace runtime

template<auto Mask, some_register Reg>
  requires field_mask_for<Mask, Reg>
[[nodiscard]] auto read(Reg const& reg) -> value_type<Reg> {
  using U = std::remove_cv_t<Reg>;
  return runtime::read(reg, static_cast<U>(Mask));
}

template<auto Mask, some_writable_register Reg>
  requires field_mask_for<Mask, Reg>
void write(Reg& reg, value_type<Reg> value) {
  using U = std::remove_cv_t<Reg>;
  runtime::write(reg, static_cast<U>(Mask), value);
}

template<auto Mask, some_writable_register Reg, typename E>
  requires(field_mask_for<Mask, Reg> && std::is_scoped_enum_v<E>)
void write(Reg& reg, E value) {
  write<Mask>(reg, static_cast<value_type<Reg>>(std::to_underlying(value)));
}

template<auto Mask, some_writable_register Reg>
  requires mask_for<Mask, Reg>
void set(Reg& reg) {
  using U = std::remove_cv_t<Reg>;
  runtime::set(reg, static_cast<U>(Mask));
}

template<auto Mask, some_writable_register Reg>
  requires mask_for<Mask, Reg>
void clear(Reg& reg) {
  using U = std::remove_cv_t<Reg>;
  runtime::clear(reg, static_cast<U>(Mask));
}

template<auto Mask, some_writable_register Reg>
  requires mask_for<Mask, Reg>
void toggle(Reg& reg) {
  using U = std::remove_cv_t<Reg>;
  runtime::toggle(reg, static_cast<U>(Mask));
}

template<auto Mask, some_register Reg>
  requires mask_for<Mask, Reg>
[[nodiscard]] auto test_any(Reg const& reg) -> bool {
  using U = std::remove_cv_t<Reg>;
  return runtime::test_any(reg, static_cast<U>(Mask));
}

template<auto Mask, some_register Reg>
  requires mask_for<Mask, Reg>
[[nodiscard]] auto test_all(Reg const& reg) -> bool {
  using U = std::remove_cv_t<Reg>;
  return runtime::test_all(reg, static_cast<U>(Mask));
}

template<auto Mask, some_register Reg>
  requires flag_mask_for<Mask, Reg>
[[nodiscard]] auto test(Reg const& reg) -> bool {
  using U = std::remove_cv_t<Reg>;
  return runtime::test_any(reg, static_cast<U>(Mask));
}

template<auto Mask, some_writable_register Reg>
  requires mask_for<Mask, Reg>
void clear_w1(Reg& reg) {
  using U = std::remove_cv_t<Reg>;
  runtime::clear_w1(reg, static_cast<U>(Mask));
}

template<auto Mask, some_writable_register Reg>
  requires mask_for<Mask, Reg>
void clear_w0(Reg& reg) {
  using U = std::remove_cv_t<Reg>;
  runtime::clear_w0(reg, static_cast<U>(Mask));
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

  template<typename E>
    requires std::is_scoped_enum_v<E>
  constexpr explicit bits(E v)
      : bits(static_cast<value_type>(std::to_underlying(v))) {}
};

template<some_writable_register Reg, typename First, typename... Rest>
void modify(Reg& reg, First first, Rest... rest) {
  using U = std::remove_cv_t<Reg>;
  static_assert(
      field_mask_for<First::mask, Reg>
          && (field_mask_for<Rest::mask, Reg> && ...),
      "field mask incompatible with this register"
  );
  constexpr auto mask_or = static_cast<U>((First::mask | ... | Rest::mask));
  constexpr auto mask_sum = static_cast<U>(
      (static_cast<U>(First::mask) + ... + static_cast<U>(Rest::mask))
  );

  static_assert(mask_or == mask_sum, "overlapping field masks");

  reg = static_cast<U>((reg & ~mask_or) | (first.encoded | ... | rest.encoded));
}

template<auto Mask, some_writable_register Reg>
  requires mask_for<Mask, Reg>
void set_or_clear(Reg& reg, bool cond) {
  if (cond)
    set<Mask>(reg);
  else
    clear<Mask>(reg);
}

} // namespace mmio
} // namespace emb
