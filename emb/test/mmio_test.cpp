#include <emb/mmio.hpp>

#include <cassert>

namespace {

constexpr bool test_mmio() {
  return true;
}

static_assert(test_mmio());

} // namespace
