#ifdef __cpp_constexpr

#include "tests.hpp"

#include <emb/generator.hpp>

namespace emb {
namespace internal {
namespace tests {

// template<typename T>
// constexpr T

constexpr bool test_sine_generator(
    SineGenerator auto generator,
    emb::units::angle_t init_phase) {
  using output_type = decltype(generator)::output_type;

  EMB_CONSTEXPR_ASSERT(generator.output() == 10 * emb::sinf(init_phase.rad().numval()));

  return true;
}

static_assert(test_sine_generator(emb::sine_generator<float>{0.125f, 10, 1}, emb::units::angle_t{}));


} // namespace tests
} // namespace internal
} // namespace emb

#endif
