#ifdef __cpp_constexpr

#include <emblib/tests/tests.hpp>

#include <emblib/filter/moving_average_filter.hpp>
#include <emblib/units.hpp>

#include <random>

namespace emb {
namespace internal {
namespace tests {
const bool b = std::is_integral_v<int>;
constexpr bool test_moving_average_filter(
    MovingAverageFilter auto filt,
    typename decltype(filt)::value_type init_output) {
  using value_type = decltype(filt)::value_type;

  EMB_CONSTEXPR_ASSERT(filt.output() == init_output);

  filt.set_output(value_type{-42});
  EMB_CONSTEXPR_ASSERT(filt.output() == value_type{-42});

  filt.push(value_type{42});

  // std::array<value_type, 7> input{10, 9, 8, 7, 6, 5, 4};
  // size_t idx{0};
  // int sum{0};

  // while (!filt.data().full()) {
  //   auto val = input[idx];
  //   filt.push(val);
  //   idx = (idx + 1) % input.size();
  //   sum += val;
  //   EMB_CONSTEXPR_ASSERT(
  //       filt.output() == sum / static_cast<int>(filt.data().size()));
  // }

  // filt.push(16);
  // EMB_CONSTEXPR_ASSERT(filt.output() == 16);

  // filt.push(8);
  // EMB_CONSTEXPR_ASSERT(filt.output() == 12);

  // filt.push(0);
  // EMB_CONSTEXPR_ASSERT(filt.output() == 8);

  // filt.push(16);
  // EMB_CONSTEXPR_ASSERT(filt.output() == 10);

  // filt.push(12);
  // EMB_CONSTEXPR_ASSERT(filt.output() == 9);

  // filt.push(-32);
  // EMB_CONSTEXPR_ASSERT(filt.output() == -1);

  // filt.reset();
  // EMB_CONSTEXPR_ASSERT(filt.output() == 42);

  // filt.push(16);
  // EMB_CONSTEXPR_ASSERT(filt.output() == 16);

  // filt.push(8);
  // EMB_CONSTEXPR_ASSERT(filt.output() == 12);

  return true;
}

static_assert(test_moving_average_filter(emb::moving_average_filter<emb::units::erad_t, 3>{}, emb::units::erad_t{0}));
static_assert(test_moving_average_filter(emb::v1::moving_average_filter<emb::units::erad_t, 3>{}, emb::units::erad_t{0}));

static_assert(
    test_moving_average_filter(emb::moving_average_filter<int, 1>{}, 0));
static_assert(
    test_moving_average_filter(emb::moving_average_filter<int>{1}, 0));
static_assert(
    test_moving_average_filter(emb::v1::moving_average_filter<int, 1>{}, 0));

static_assert(
    test_moving_average_filter(emb::moving_average_filter<int, 2>{}, 0));
static_assert(
    test_moving_average_filter(emb::moving_average_filter<int>{2}, 0));
static_assert(
    test_moving_average_filter(emb::v1::moving_average_filter<int, 2>{}, 0));

static_assert(
    test_moving_average_filter(emb::moving_average_filter<int, 5>{}, 0));
static_assert(
    test_moving_average_filter(emb::moving_average_filter<int>{5}, 0));
static_assert(
    test_moving_average_filter(emb::v1::moving_average_filter<int, 5>{}, 0));

static_assert(
    test_moving_average_filter(emb::moving_average_filter<int, 10>{}, 0));
static_assert(
    test_moving_average_filter(emb::moving_average_filter<int>{10}, 0));
static_assert(
    test_moving_average_filter(emb::v1::moving_average_filter<int, 10>{}, 0));

static_assert(
    test_moving_average_filter(emb::moving_average_filter<int, 1>{42}, 42));
static_assert(
    test_moving_average_filter(emb::moving_average_filter<int>{1, 42}, 42));
static_assert(
    test_moving_average_filter(emb::v1::moving_average_filter<int, 1>{42}, 42));

static_assert(
    test_moving_average_filter(emb::moving_average_filter<int, 2>{42}, 42));
static_assert(
    test_moving_average_filter(emb::moving_average_filter<int>{2, 42}, 42));
static_assert(
    test_moving_average_filter(emb::v1::moving_average_filter<int, 2>{42}, 42));

static_assert(
    test_moving_average_filter(emb::moving_average_filter<int, 5>{42}, 42));
static_assert(
    test_moving_average_filter(emb::moving_average_filter<int>{5, 42}, 42));
static_assert(
    test_moving_average_filter(emb::v1::moving_average_filter<int, 5>{42}, 42));

static_assert(
    test_moving_average_filter(emb::moving_average_filter<int, 10>{42}, 42));
static_assert(
    test_moving_average_filter(emb::moving_average_filter<int>{10, 42}, 42));
static_assert(test_moving_average_filter(
    emb::v1::moving_average_filter<int, 10>{42},
    42));

} // namespace tests
} // namespace internal
} // namespace emb

#endif
