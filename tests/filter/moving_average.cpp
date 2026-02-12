#include <emb/filter.hpp>
#include <emb/units.hpp>

#include <numeric>

namespace emb {
namespace tests {

constexpr bool test_moving_average_filter(
    emb::filter::moving_average_filter_type auto filter,
    typename decltype(filter)::value_type init_output
) {
  using value_type = decltype(filter)::value_type;
  using divider_type = decltype(filter)::divider_type;

  assert(filter.output() == init_output);

  filter.set_output(value_type{-42});
  assert(filter.output() == value_type{-42});

  std::array<value_type, 7> input{
      value_type{10},
      value_type{9},
      value_type{8},
      value_type{7},
      value_type{6},
      value_type{5},
      value_type{4}
  };
  size_t idx{0};
  value_type sum{0};

  while (!filter.data().full()) {
    auto val = input[idx];
    filter.push(val);
    idx = (idx + 1) % input.size();
    sum += val;
    [[maybe_unused]] auto out = sum /
                                static_cast<divider_type>(filter.data().size());
    assert(filter.output() == out);
  }

  auto const capacity = filter.data().capacity();
  for (auto i{0uz}; i < 2 * capacity + capacity / 2; ++i) {
    auto val = input[idx];
    filter.push(val);
    idx = (idx + 1) % input.size();
  }

  sum = std::accumulate(filter.data().begin(), filter.data().end(), value_type{0});
  assert(filter.output() == sum / static_cast<divider_type>(capacity));

  filter.reset();
  assert(filter.output() == init_output);

  return true;
}

static_assert(
    test_moving_average_filter(emb::filter::moving_average<int, 1>{}, 0)
);
static_assert(
    test_moving_average_filter(emb::filter::moving_average<int, 2>{}, 0)
);
static_assert(
    test_moving_average_filter(emb::filter::moving_average<int, 5>{}, 0)
);
static_assert(
    test_moving_average_filter(emb::filter::moving_average<int, 10>{}, 0)
);

static_assert(
    test_moving_average_filter(emb::filter::moving_average<int, 1>{42}, 42)
);
static_assert(
    test_moving_average_filter(emb::filter::moving_average<int, 2>{42}, 42)
);
static_assert(
    test_moving_average_filter(emb::filter::moving_average<int, 5>{42}, 42)
);
static_assert(
    test_moving_average_filter(emb::filter::moving_average<int, 10>{42}, 42)
);

static_assert(test_moving_average_filter(
    emb::filter::moving_average<emb::units::erad_f32, 4>{},
    emb::units::erad_f32{0}
));

} // namespace tests
} // namespace emb
