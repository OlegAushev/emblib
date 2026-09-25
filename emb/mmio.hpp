#pragma once

#include <emb/meta.hpp>

#include <bit>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace emb {
namespace mmio {

// The concept `some_register<Reg>` is satisfied if and only if `Reg` is a
// register type, i.e. `std::uint8_t`, `std::uint16_t`, `std::uint32_t` or
// `std::uint64_t`, possibly cv-qualified. Other types of the same width are
// not register types. An object of a register type is a register.
template<typename Reg>
concept some_register = emb::same_as_any<std::remove_cv_t<Reg>,
                                         std::uint8_t,
                                         std::uint16_t,
                                         std::uint32_t,
                                         std::uint64_t>;

// The concept `some_writable_register<Reg>` is satisfied if and only if `Reg`
// satisfies `some_register` and is not const-qualified.
template<typename Reg>
concept some_writable_register = some_register<Reg> && !std::is_const_v<Reg>;

// Type of a mask for a register of type `Reg`, i.e. of a value whose set bits
// select the bits of the register that an operation acts on.
template<typename Reg>
using mask_type = std::remove_cv_t<Reg>;

// Type of the value of a field of a register of type `Reg`.
template<typename Reg>
using value_type = std::remove_cv_t<Reg>;

namespace detail {

// Checks whether the set bits of `mask` are contiguous. The behavior is
// undefined if `mask` is zero.
template<std::unsigned_integral M>
constexpr bool is_contiguous_mask(M mask)
{
  M const x = static_cast<M>(mask >> std::countr_zero(mask));
  return (x & (x + M{1})) == M{0};
}

} // namespace detail

// The concept `valid_mask<Mask>` is satisfied if and only if `Mask` is a
// positive value of type `unsigned char`, `unsigned short`, `unsigned int`,
// `unsigned long` or `unsigned long long`.
template<auto Mask>
concept valid_mask = emb::same_as_any<decltype(Mask),
                                      unsigned char,
                                      unsigned short,
                                      unsigned int,
                                      unsigned long,
                                      unsigned long long>
                  && (Mask > 0);

// The concept `field_mask<Mask>` is satisfied if and only if `Mask` satisfies
// `valid_mask` and its set bits are contiguous. Such a mask selects a field,
// whose value is its bits shifted down to bit 0.
template<auto Mask>
concept field_mask = valid_mask<Mask> && detail::is_contiguous_mask(Mask);

// The concept `mask_for<Mask, Reg>` is satisfied if and only if `Reg`
// satisfies `some_register`, `Mask` satisfies `valid_mask`, and the value of
// `Mask` fits in `Reg`, even if the type of `Mask` is wider than `Reg`.
template<auto Mask, typename Reg>
concept mask_for = some_register<Reg>
                && valid_mask<Mask>
                && std::in_range<std::remove_cv_t<Reg>>(Mask);

// The concept `field_mask_for<Mask, Reg>` is satisfied if and only if both
// `mask_for<Mask, Reg>` and `field_mask<Mask>` are satisfied, i.e. `Mask`
// selects a field of a register of type `Reg`.
template<auto Mask, typename Reg>
concept field_mask_for = mask_for<Mask, Reg> && field_mask<Mask>;

// The concept `flag_mask_for<Mask, Reg>` is satisfied if and only if
// `mask_for<Mask, Reg>` is satisfied and `Mask` has exactly one bit set. Such a
// mask selects a flag, i.e. a field of one bit.
template<auto Mask, typename Reg>
concept flag_mask_for =
    mask_for<Mask, Reg>
    && std::has_single_bit(static_cast<std::remove_cv_t<Reg>>(Mask));

namespace runtime {

// Returns the value of the field of `reg` selected by `mask`. Reads `reg` once.
// If `mask` is not contiguous, returns the bits of `reg` selected by `mask`
// shifted right by the index of the lowest set bit of `mask`, with zeros in
// the gaps. If `mask` is zero, the behavior is undefined for a 32- or 64-bit
// register, and the result is zero for an 8- or 16-bit one.
template<some_register Reg>
[[nodiscard]] auto read(Reg const& reg, mask_type<Reg> mask) -> value_type<Reg>
{
  using U = std::remove_cv_t<Reg>;
  return static_cast<U>((reg & mask) >> std::countr_zero(mask));
}

// Writes the low bits of `value` to the field of `reg` selected by `mask`.
// Reads `reg` once and writes it once, not atomically: a change made in between
// is overwritten. A flag outside `mask` that reads as 1 is cleared if writing 1
// clears it. If `mask` is not contiguous, writes `value` shifted left by the
// index of the lowest set bit of `mask` to the bits selected by `mask`; bits of
// `value` that fall into the gaps are discarded. If `mask` is zero, the
// behavior is undefined for a 32- or 64-bit register, and an 8- or 16-bit one
// is written back as read.
template<some_writable_register Reg>
void write(Reg& reg, mask_type<Reg> mask, value_type<Reg> value)
{
  using U = std::remove_cv_t<Reg>;
  reg = static_cast<U>((reg & ~mask)
                       | ((value << std::countr_zero(mask)) & mask));
}

// Sets the bits of `reg` selected by `mask`. Reads `reg` once and writes it
// once, not atomically: a change made in between is overwritten. A flag outside
// `mask` that reads as 1 is cleared if writing 1 clears it. `mask` may be zero.
template<some_writable_register Reg>
void set(Reg& reg, mask_type<Reg> mask)
{
  using U = std::remove_cv_t<Reg>;
  reg = static_cast<U>(reg | mask);
}

// Clears the bits of `reg` selected by `mask`. Reads `reg` once and writes it
// once, not atomically: a change made in between is overwritten. A flag outside
// `mask` that reads as 1 is cleared if writing 1 clears it. `mask` may be zero.
template<some_writable_register Reg>
void clear(Reg& reg, mask_type<Reg> mask)
{
  using U = std::remove_cv_t<Reg>;
  reg = static_cast<U>(reg & ~mask);
}

// Inverts the bits of `reg` selected by `mask`. Reads `reg` once and writes it
// once, not atomically: a change made in between is overwritten. A flag outside
// `mask` that reads as 1 is cleared if writing 1 clears it. `mask` may be zero.
template<some_writable_register Reg>
void toggle(Reg& reg, mask_type<Reg> mask)
{
  using U = std::remove_cv_t<Reg>;
  reg = static_cast<U>(reg ^ mask);
}

// Checks whether any of the bits of `reg` selected by `mask` are set. Reads
// `reg` once. Returns `false` if `mask` is zero.
template<some_register Reg>
[[nodiscard]] auto test_any(Reg const& reg, mask_type<Reg> mask) -> bool
{
  return (reg & mask) != 0;
}

// Checks whether all of the bits of `reg` selected by `mask` are set. Reads
// `reg` once. Returns `true` if `mask` is zero.
template<some_register Reg>
[[nodiscard]] auto test_all(Reg const& reg, mask_type<Reg> mask) -> bool
{
  return (reg & mask) == mask;
}

// Clears the flags of `reg` selected by `mask` if writing 1 clears them, and
// writes 0 to the bits outside `mask`. Writes `reg` once without reading it.
// `mask` may be zero.
template<some_writable_register Reg>
void clear_w1(Reg& reg, mask_type<Reg> mask)
{
  using U = std::remove_cv_t<Reg>;
  reg = static_cast<U>(mask);
}

// Clears the flags of `reg` selected by `mask` if writing 0 clears them, and
// writes 1 to the bits outside `mask`. Writes `reg` once without reading it.
// `mask` may be zero.
template<some_writable_register Reg>
void clear_w0(Reg& reg, mask_type<Reg> mask)
{
  using U = std::remove_cv_t<Reg>;
  reg = static_cast<U>(~mask);
}

// Sets the bits of `reg` selected by `mask` if `cond` is `true` and clears them
// otherwise. Reads `reg` once and writes it once, not atomically: a change made
// in between is overwritten. A flag outside `mask` that reads as 1 is cleared
// if writing 1 clears it. `mask` may be zero.
template<some_writable_register Reg>
void set_or_clear(Reg& reg, mask_type<Reg> mask, bool cond)
{
  if (cond)
    set(reg, mask);
  else
    clear(reg, mask);
}

} // namespace runtime

// Returns the value of the field of `reg` selected by `Mask`. Reads `reg` once.
template<auto Mask, some_register Reg>
  requires field_mask_for<Mask, Reg>
[[nodiscard]] auto read(Reg const& reg) -> value_type<Reg>
{
  using U = std::remove_cv_t<Reg>;
  return runtime::read(reg, static_cast<U>(Mask));
}

// Writes the low bits of `value` to the field of `reg` selected by `Mask`.
// Reads `reg` once and writes it once, not atomically: a change made in between
// is overwritten. A flag outside `Mask` that reads as 1 is cleared if writing 1
// clears it.
template<auto Mask, some_writable_register Reg>
  requires field_mask_for<Mask, Reg>
void write(Reg& reg, value_type<Reg> value)
{
  using U = std::remove_cv_t<Reg>;
  runtime::write(reg, static_cast<U>(Mask), value);
}

// Writes the low bits of the underlying value of `value` to the field of `reg`
// selected by `Mask`. Reads `reg` once and writes it once, not atomically: a
// change made in between is overwritten. A flag outside `Mask` that reads as 1
// is cleared if writing 1 clears it.
template<auto Mask, some_writable_register Reg, typename E>
  requires(field_mask_for<Mask, Reg> && std::is_scoped_enum_v<E>)
void write(Reg& reg, E value)
{
  write<Mask>(reg, static_cast<value_type<Reg>>(std::to_underlying(value)));
}

// Sets the bits of `reg` selected by `Mask`. Reads `reg` once and writes it
// once, not atomically: a change made in between is overwritten. A flag outside
// `Mask` that reads as 1 is cleared if writing 1 clears it.
template<auto Mask, some_writable_register Reg>
  requires mask_for<Mask, Reg>
void set(Reg& reg)
{
  using U = std::remove_cv_t<Reg>;
  runtime::set(reg, static_cast<U>(Mask));
}

// Clears the bits of `reg` selected by `Mask`. Reads `reg` once and writes it
// once, not atomically: a change made in between is overwritten. A flag outside
// `Mask` that reads as 1 is cleared if writing 1 clears it.
template<auto Mask, some_writable_register Reg>
  requires mask_for<Mask, Reg>
void clear(Reg& reg)
{
  using U = std::remove_cv_t<Reg>;
  runtime::clear(reg, static_cast<U>(Mask));
}

// Inverts the bits of `reg` selected by `Mask`. Reads `reg` once and writes it
// once, not atomically: a change made in between is overwritten. A flag outside
// `Mask` that reads as 1 is cleared if writing 1 clears it.
template<auto Mask, some_writable_register Reg>
  requires mask_for<Mask, Reg>
void toggle(Reg& reg)
{
  using U = std::remove_cv_t<Reg>;
  runtime::toggle(reg, static_cast<U>(Mask));
}

// Checks whether any of the bits of `reg` selected by `Mask` are set. Reads
// `reg` once.
template<auto Mask, some_register Reg>
  requires mask_for<Mask, Reg>
[[nodiscard]] auto test_any(Reg const& reg) -> bool
{
  using U = std::remove_cv_t<Reg>;
  return runtime::test_any(reg, static_cast<U>(Mask));
}

// Checks whether all of the bits of `reg` selected by `Mask` are set. Reads
// `reg` once.
template<auto Mask, some_register Reg>
  requires mask_for<Mask, Reg>
[[nodiscard]] auto test_all(Reg const& reg) -> bool
{
  using U = std::remove_cv_t<Reg>;
  return runtime::test_all(reg, static_cast<U>(Mask));
}

// Checks whether the flag of `reg` selected by `Mask` is set. Reads `reg` once.
template<auto Mask, some_register Reg>
  requires flag_mask_for<Mask, Reg>
[[nodiscard]] auto test(Reg const& reg) -> bool
{
  using U = std::remove_cv_t<Reg>;
  return runtime::test_any(reg, static_cast<U>(Mask));
}

// Clears the flags of `reg` selected by `Mask` if writing 1 clears them, and
// writes 0 to the bits outside `Mask`. Writes `reg` once without reading it.
template<auto Mask, some_writable_register Reg>
  requires mask_for<Mask, Reg>
void clear_w1(Reg& reg)
{
  using U = std::remove_cv_t<Reg>;
  runtime::clear_w1(reg, static_cast<U>(Mask));
}

// Clears the flags of `reg` selected by `Mask` if writing 0 clears them, and
// writes 1 to the bits outside `Mask`. Writes `reg` once without reading it.
template<auto Mask, some_writable_register Reg>
  requires mask_for<Mask, Reg>
void clear_w0(Reg& reg)
{
  using U = std::remove_cv_t<Reg>;
  runtime::clear_w0(reg, static_cast<U>(Mask));
}

// The class template `bits` holds a value for the field selected by `Mask`,
// shifted into the field's position, for `modify` to write to a register. A
// value that does not fit in the field is truncated to the width of the field;
// a scoped enumeration value is converted to its underlying value first.
template<auto Mask>
  requires field_mask<Mask>
struct bits {
  using value_type = decltype(Mask);
  static constexpr value_type mask = Mask;
  static constexpr unsigned offset = std::countr_zero(mask);

  value_type encoded;

  constexpr explicit bits(value_type v)
      : encoded(value_type((v << offset) & mask))
  {
  }

  template<typename E>
    requires std::is_scoped_enum_v<E>
  constexpr explicit bits(E v)
      : bits(static_cast<value_type>(std::to_underlying(v)))
  {
  }
};

// Writes the values held by `first` and `rest`, which are `bits` objects or
// have the same members `mask` and `encoded`, to the fields of `reg` selected
// by their masks. Reads `reg` once and writes it once, not atomically: a change
// made in between is overwritten. A flag outside the masks that reads as 1 is
// cleared if writing 1 clears it. The program is ill-formed if a mask does not
// fit in `reg` or two masks overlap.
template<some_writable_register Reg, typename First, typename... Rest>
void modify(Reg& reg, First first, Rest... rest)
{
  using U = std::remove_cv_t<Reg>;
  static_assert(field_mask_for<First::mask, Reg>
                    && (field_mask_for<Rest::mask, Reg> && ...),
                "field mask incompatible with this register");
  constexpr auto mask_or = static_cast<U>((First::mask | ... | Rest::mask));
  constexpr int bit_count = (std::popcount(static_cast<U>(First::mask))
                             + ...
                             + std::popcount(static_cast<U>(Rest::mask)));

  static_assert(std::popcount(mask_or) == bit_count, "overlapping field masks");

  reg = static_cast<U>((reg & ~mask_or) | (first.encoded | ... | rest.encoded));
}

// Sets the bits of `reg` selected by `Mask` if `cond` is `true` and clears them
// otherwise. Reads `reg` once and writes it once, not atomically: a change made
// in between is overwritten. A flag outside `Mask` that reads as 1 is cleared
// if writing 1 clears it.
template<auto Mask, some_writable_register Reg>
  requires mask_for<Mask, Reg>
void set_or_clear(Reg& reg, bool cond)
{
  if (cond)
    set<Mask>(reg);
  else
    clear<Mask>(reg);
}

} // namespace mmio
} // namespace emb
