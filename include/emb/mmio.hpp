#pragma once

#ifdef __cpp_concepts

#include <emb/core.hpp>

#include <bit>
#include <concepts>

namespace emb {
namespace mmio {
namespace cmsis {

// Concept: Peripheral must have regs_type and regs.
template<typename Peripheral>
concept peripheral = requires {
  typename Peripheral::regs_type;
  { *Peripheral::regs } -> std::same_as<typename Peripheral::regs_type&>;
};

// Concept: Register must be valid member pointer for Peripheral::regs_type.
template<typename Peripheral, auto Reg>
concept valid_reg = peripheral<Peripheral> &&
                    requires(Peripheral::regs_type* regs) {
                      { regs->*Reg };
                    };

// Helper type (needs valid_reg to be safe).
template<peripheral Peripheral, auto Reg>
  requires valid_reg<Peripheral, Reg>
using reg_type = std::remove_cvref_t<decltype(Peripheral::regs->*Reg)>;

// Concept: Mask must be convertible to register type.
template<typename Peripheral, auto Reg, auto Mask>
concept valid_mask =
    valid_reg<Peripheral, Reg> &&
    std::convertible_to<decltype(Mask), reg_type<Peripheral, Reg>>;

template<peripheral Peripheral, auto Reg, auto Mask>
  requires valid_mask<Peripheral, Reg, Mask>
[[nodiscard]] auto read() {
  using T = reg_type<Peripheral, Reg>;
  constexpr auto mask = static_cast<T>(Mask);
  constexpr auto offset = std::countr_zero(mask);
  auto& reg = Peripheral::regs->*Reg;
  return (reg & mask) >> offset;
}
template<peripheral Peripheral, auto Reg, auto Mask>
  requires valid_mask<Peripheral, Reg, Mask>
void write(reg_type<Peripheral, Reg> val) {
  using T = reg_type<Peripheral, Reg>;
  constexpr auto mask = static_cast<T>(Mask);
  constexpr auto offset = std::countr_zero(mask);
  auto& reg = Peripheral::regs->*Reg;
  reg = (reg & ~mask) | ((val << offset) & mask);
}

template<peripheral Peripheral, auto Reg>
  requires valid_reg<Peripheral, Reg>
[[nodiscard]] auto read() {
  return Peripheral::regs->*Reg;
}

template<peripheral Peripheral, auto Reg>
  requires valid_reg<Peripheral, Reg>
void write(reg_type<Peripheral, Reg> val) {
  Peripheral::regs->*Reg = val;
}

template<typename Policy, peripheral Peripheral, auto Reg, auto Mask>
  requires valid_mask<Peripheral, Reg, Mask>
void set() {
  using T = reg_type<Peripheral, Reg>;
  Policy::set(Peripheral::regs->*Reg, static_cast<T>(Mask));
}

template<typename Policy, peripheral Peripheral, auto Reg, auto Mask>
  requires valid_mask<Peripheral, Reg, Mask>
void clear() {
  using T = reg_type<Peripheral, Reg>;
  Policy::clear(Peripheral::regs->*Reg, static_cast<T>(Mask));
}

// Policies define how to set/clear bits.
struct rw {
  template<typename Reg, typename Mask>
  static void set(Reg& reg, Mask mask) {
    reg |= mask;
  }

  template<typename Reg, typename Mask>
  static void clear(Reg& reg, Mask mask) {
    reg &= ~mask;
  }
};

struct rc_w0 {
  template<typename Reg, typename Mask>
  static void clear(Reg& reg, Mask mask) {
    reg = ~mask;
  }
};

struct rc_w1 {
  template<typename Reg, typename Mask>
  static void clear(Reg& reg, Mask mask) {
    reg = mask;
  }
};

} // namespace cmsis
} // namespace mmio
} // namespace emb

#define EMB_MMIO_CMSIS_REG(name) static constexpr auto name = &regs_type::name

#endif
