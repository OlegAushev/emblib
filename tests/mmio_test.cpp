#ifdef __cpp_constexpr

#include <emb/mmio.hpp>

#include <cassert>

namespace emb {
namespace internal {
namespace tests {

// struct tim {
//   using regs_type = TMR_TypeDef;
//   static inline regs_type* const regs = TMR1;
//   EMB_MMIO_CMSIS_REG(CTRL1);
// };

// static_assert(mmio::cmsis::peripheral<tim>);

// inline void test() {
//   mmio::cmsis::write<tim, tim::CTRL1, TMR_CTRL1_CNTEN>(1);
// }

struct mock_regs {
  uint32_t ctrl;
  uint32_t status;
  uint8_t data;
};

struct mock_peripheral {
  using regs_type = mock_regs;
  static regs_type* regs;
  static constexpr void init(mock_regs& r) {
    regs = &r;
  }
  EMB_MMIO_CMSIS_REG(ctrl);
  EMB_MMIO_CMSIS_REG(status);
  EMB_MMIO_CMSIS_REG(data);
};

static_assert(mmio::cmsis::peripheral<mock_peripheral>);

constexpr bool test_mmio() {





  // mock_regs r;
  // mock_peripheral::init(r);

  // mmio::cmsis::write<mock_peripheral, mock_peripheral::ctrl, 0x00000FF0>(42);
  // [[maybe_unused]] auto val = mmio::cmsis::read<tim, tim::ctrl, 0x10>(1);
  // assert(val == 42);

  // uint32_t reg32 = 0;
  // mmio::write()  _reg<mmio::rw>(reg32, 0x12345678u);
  // assert(mmio::read_reg<mmio::rw>(reg32) == 0x12345678);

  return true;
}

static_assert(test_mmio());

}
}
}

#endif
