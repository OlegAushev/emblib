#ifdef __cpp_constexpr

#include <emblib/tests/tests.hpp>

#include <emblib/filter/moving_average_filter.hpp>

namespace emb {
namespace internal {
namespace tests {

constexpr bool test_moving_average_filter(MovingAverageFilter auto filt)
  requires(std::same_as<typename decltype(filt)::value_type, int>) {
  EMB_CONSTEXPR_ASSERT(filt.output() == 42);

  filt.set_output(-42);
  EMB_CONSTEXPR_ASSERT(filt.output() == -42);

  filt.push(16);
  EMB_CONSTEXPR_ASSERT(filt.output() == 16);

  filt.push(8);
  EMB_CONSTEXPR_ASSERT(filt.output() == 12);

  filt.push(0);
  EMB_CONSTEXPR_ASSERT(filt.output() == 8);

  filt.push(16);
  EMB_CONSTEXPR_ASSERT(filt.output() == 10);

  filt.push(12);
  EMB_CONSTEXPR_ASSERT(filt.output() == 9);

  filt.push(-32);
  EMB_CONSTEXPR_ASSERT(filt.output() == -1);

  filt.reset();
  EMB_CONSTEXPR_ASSERT(filt.output() == 42);

  return true;
}

static_assert(test_moving_average_filter(emb::moving_average_filter<int, 4>{42}));
static_assert(test_moving_average_filter(emb::moving_average_filter<int>{4, 42}));
static_assert(
    test_moving_average_filter(emb::v1::moving_average_filter<int, 4>{42}));

} // namespace tests
} // namespace internal
} // namespace emb

#endif
