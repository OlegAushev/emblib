#include <emb/filter/moving_average_filter.hpp>
#include <emb/units.hpp>

#include <cstddef>

namespace {

template<typename Filter>
constexpr bool
test_moving_average_filter(Filter filter,
                           typename Filter::value_type init_output)
{
  using value_type = Filter::value_type;
  using divider_type = Filter::divider_type;

  assert(filter.output() == init_output);

  filter.set_output(value_type{-42});
  assert(filter.output() == value_type{-42});

  std::array<value_type, 7> input{value_type{10},
                                  value_type{9},
                                  value_type{8},
                                  value_type{7},
                                  value_type{6},
                                  value_type{5},
                                  value_type{4}};
  std::size_t idx{0};
  value_type sum{0};

  while (!filter.data().full()) {
    auto val = input[idx];
    filter.push(val);
    idx = (idx + 1) % input.size();
    sum += val;
    [[maybe_unused]] auto out =
        sum / static_cast<divider_type>(filter.data().size());
    assert(filter.output() == out);
  }

  auto const capacity = filter.data().capacity();
  for (auto i{0uz}; i < 2 * capacity + capacity / 2; ++i) {
    auto val = input[idx];
    filter.push(val);
    idx = (idx + 1) % input.size();
  }

  sum = value_type{0};
  for (auto i = 0uz; i < filter.data().size(); ++i) {
    sum += filter.data()[i];
  }
  assert(filter.output() == sum / static_cast<divider_type>(capacity));

  filter.reset();
  assert(filter.output() == init_output);

  return true;
}

static_assert(test_moving_average_filter(emb::moving_average_filter<int, 1>{},
                                         0));
static_assert(test_moving_average_filter(emb::moving_average_filter<int, 2>{},
                                         0));
static_assert(test_moving_average_filter(emb::moving_average_filter<int, 5>{},
                                         0));
static_assert(test_moving_average_filter(emb::moving_average_filter<int, 10>{},
                                         0));

static_assert(test_moving_average_filter(emb::moving_average_filter<int, 1>{42},
                                         42));
static_assert(test_moving_average_filter(emb::moving_average_filter<int, 2>{42},
                                         42));
static_assert(test_moving_average_filter(emb::moving_average_filter<int, 5>{42},
                                         42));
static_assert(
    test_moving_average_filter(emb::moving_average_filter<int, 10>{42}, 42));

static_assert(test_moving_average_filter(
    emb::moving_average_filter<emb::units::erad_f32, 4>{},
    emb::units::erad_f32{0}));

// A value much larger than the rest no longer affects the output
// `WindowSize - 1` pushes after it has left the window, wherever it falls
// among the pushes. An infinity or a NaN can't be tested here: constant
// evaluation rejects arithmetic that produces a NaN.
template<std::size_t WindowSize>
constexpr bool test_moving_average_outlier(float outlier)
{
  for (auto offset = 0uz; offset < WindowSize; ++offset) {
    emb::moving_average_filter<float, WindowSize> filter;
    for (auto i = 0uz; i < WindowSize + offset; ++i) {
      filter.push(1.0f);
    }
    filter.push(outlier);
    for (auto i = 0uz; i < 2 * WindowSize - 1; ++i) {
      filter.push(1.0f);
    }
    assert(filter.output() == 1.0f);
  }
  return true;
}

static_assert(test_moving_average_outlier<4>(1e8f));
static_assert(test_moving_average_outlier<16>(1e8f));

} // namespace
