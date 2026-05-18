#ifdef __cpp_constexpr

#include <emb/mmio.hpp>

#include <cassert>

namespace emb {
namespace internal {
namespace tests {

constexpr bool test_mmio() {
  return true;
}

static_assert(test_mmio());

} // namespace tests
} // namespace internal
} // namespace emb

#endif
