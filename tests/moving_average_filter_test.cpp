#ifdef __cpp_constexpr

#include <emblib/tests/tests.hpp>

#include <emblib/circular_buffer.hpp>

namespace emb {
namespace internal {
namespace tests {

constexpr bool test_moving_average_filter(MovingAverageFilter auto buf)
  requires(std::same_as<typename decltype(buf)::value_type, int>)
{
  return true;
}

static_assert(test_moving_average_filter(emb::moving_average_filter<int, 4>{}));
static_assert(test_moving_average_filter(emb::moving_average_filter<int>{4}));
static_assert(
    test_moving_average_filter(emb::v1::moving_average_filter<int, 4>{}));

} // namespace tests
} // namespace internal
} // namespace emb

#endif
