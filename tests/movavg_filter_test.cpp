#ifdef __cpp_constexpr

#include "tests.hpp"

#include <emb/filter.hpp>
#include <emb/units.hpp>

#include <numeric>

namespace emb {
namespace internal {
namespace tests {

constexpr bool test_movavg_filter(
    moving_average_filter_type auto filt,
    typename decltype(filt)::value_type init_output) {
  using value_type = decltype(filt)::value_type;
  using divider_type = decltype(filt)::divider_type;

  EMB_CONSTEXPR_ASSERT(filt.output() == init_output);

  filt.set_output(value_type{-42});
  EMB_CONSTEXPR_ASSERT(filt.output() == value_type{-42});

  std::array<value_type, 7> input{
      value_type{10},
      value_type{9},
      value_type{8},
      value_type{7},
      value_type{6},
      value_type{5},
      value_type{4}};
  size_t idx{0};
  value_type sum{0};

  while (!filt.data().full()) {
    auto val = input[idx];
    filt.push(val);
    idx = (idx + 1) % input.size();
    sum += val;
    EMB_CONSTEXPR_ASSERT(
        filt.output() == sum / static_cast<divider_type>(filt.data().size()));
  }

  for (auto i{0uz}; i < 2 * filt.window_size() + filt.window_size() / 2; ++i) {
    auto val = input[idx];
    filt.push(val);
    idx = (idx + 1) % input.size();
  }

  sum = std::accumulate(filt.data().begin(), filt.data().end(), value_type{0});
  EMB_CONSTEXPR_ASSERT(
      filt.output() == sum / static_cast<divider_type>(filt.window_size()));

  filt.reset();
  EMB_CONSTEXPR_ASSERT(filt.output() == init_output);

  return true;
}

static_assert(test_movavg_filter(emb::movavg_filter<int, 1>{}, 0));
static_assert(test_movavg_filter(emb::movavg_filter<int>{1}, 0));
static_assert(test_movavg_filter(emb::v1::movavg_filter<int, 1>{}, 0));

static_assert(test_movavg_filter(emb::movavg_filter<int, 2>{}, 0));
static_assert(test_movavg_filter(emb::movavg_filter<int>{2}, 0));
static_assert(test_movavg_filter(emb::v1::movavg_filter<int, 2>{}, 0));

static_assert(test_movavg_filter(emb::movavg_filter<int, 5>{}, 0));
static_assert(test_movavg_filter(emb::movavg_filter<int>{5}, 0));
static_assert(test_movavg_filter(emb::v1::movavg_filter<int, 5>{}, 0));

static_assert(test_movavg_filter(emb::movavg_filter<int, 10>{}, 0));
static_assert(test_movavg_filter(emb::movavg_filter<int>{10}, 0));
static_assert(test_movavg_filter(emb::v1::movavg_filter<int, 10>{}, 0));

static_assert(test_movavg_filter(emb::movavg_filter<int, 1>{42}, 42));
static_assert(test_movavg_filter(emb::movavg_filter<int>{1, 42}, 42));
static_assert(test_movavg_filter(emb::v1::movavg_filter<int, 1>{42}, 42));

static_assert(test_movavg_filter(emb::movavg_filter<int, 2>{42}, 42));
static_assert(test_movavg_filter(emb::movavg_filter<int>{2, 42}, 42));
static_assert(test_movavg_filter(emb::v1::movavg_filter<int, 2>{42}, 42));

static_assert(test_movavg_filter(emb::movavg_filter<int, 5>{42}, 42));
static_assert(test_movavg_filter(emb::movavg_filter<int>{5, 42}, 42));
static_assert(test_movavg_filter(emb::v1::movavg_filter<int, 5>{42}, 42));

static_assert(test_movavg_filter(emb::movavg_filter<int, 10>{42}, 42));
static_assert(test_movavg_filter(emb::movavg_filter<int>{10, 42}, 42));
static_assert(test_movavg_filter(emb::v1::movavg_filter<int, 10>{42}, 42));

static_assert(test_movavg_filter(
    emb::movavg_filter<emb::units::erad_f32, 4>{},
    emb::units::erad_f32{0}));
static_assert(test_movavg_filter(
    emb::movavg_filter<emb::units::erad_f32>{4},
    emb::units::erad_f32{0}));
static_assert(test_movavg_filter(
    emb::v1::movavg_filter<emb::units::erad_f32, 4>{},
    emb::units::erad_f32{0}));

} // namespace tests
} // namespace internal
} // namespace emb

#endif
