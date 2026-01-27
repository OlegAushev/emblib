#pragma once

#ifdef __cpp_concepts

#include <emb/meta.hpp>

#include <bit>
#include <concepts>
#include <cstdint>
#include <limits>

namespace emb {
namespace mmio {

template<typename T>
concept reg_address =
    emb::same_as_any<T, uint8_t, uint16_t, uint32_t, uint64_t, uintptr_t>;

template<typename T>
concept reg_width = emb::same_as_any<T, uint8_t, uint16_t, uint32_t, uint64_t>;

template<auto Address, reg_width Width = uint32_t>
  requires reg_address<decltype(Address)>
struct reg {
  using value_type = Width;

  static value_type read() {
    return *reinterpret_cast<value_type const volatile*>(Address);
  }

  static void write(value_type value) {
    *reinterpret_cast<value_type volatile*>(Address) = value;
  }
};

struct ro {
  template<typename T>
  static T read(T const volatile* reg, T mask, unsigned offset) {
    return (*reg & mask) >> offset;
  }
};

struct rw : public ro {
  template<typename T>
  static void write(T volatile* reg, T mask, unsigned offset, T value) {
    *reg = (*reg & ~mask) | ((value << offset) & mask);
  }

  template<typename T>
  static void set(T volatile* reg, T mask) {
    *reg = *reg | mask;
  }

  template<typename T>
  static void clear(T volatile* reg, T mask) {
    *reg = *reg & ~mask;
  }
};

struct rc_w0 : public ro {
  template<typename T>
  static void clear(T volatile* reg, T mask) {
    *reg = ~mask;
  }
};

struct rc_w1 : public ro {
  template<typename T>
  static void clear(T volatile* reg, T mask) {
    *reg = mask;
  }
};

template<auto Address, auto Mask, typename Policy = rw>
  requires reg_address<decltype(Address)> &&
           reg_width<decltype(Mask)> &&
           (Mask != 0)
struct field {
  using value_type = decltype(Mask);
  using pointer = value_type volatile*;
  using const_pointer = value_type const volatile*;
  using policy_type = Policy;

  static constexpr auto address = Address;
  static constexpr value_type mask = Mask;
  static constexpr unsigned offset = std::countr_zero(mask);

  static value_type read() {
    return policy_type::read(
        reinterpret_cast<const_pointer>(address),
        mask,
        offset
    );
  }

  static void write(value_type value) {
    policy_type::write(reinterpret_cast<pointer>(address), mask, offset, value);
  }

  static void set() {
    policy_type::set(reinterpret_cast<pointer>(address), mask);
  }

  static void clear() {
    policy_type::clear(reinterpret_cast<pointer>(address), mask);
  }
};

} // namespace mmio
} // namespace emb

#endif
