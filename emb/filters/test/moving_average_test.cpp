#include <emb/filters/moving_average.hpp>
#include <emb/units.hpp>

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>

namespace {

template<typename Filter>
constexpr bool
test_moving_average_filter(Filter filter,
                           typename Filter::value_type init_output)
{
  using value_type = Filter::value_type;
  using divisor_type = Filter::divisor_type;

  assert(filter.output() == init_output);

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
    filter.update(val);
    idx = (idx + 1) % input.size();
    sum += val;
    [[maybe_unused]] auto out =
        sum / static_cast<divisor_type>(filter.data().size());
    assert(filter.output() == out);
  }

  auto const capacity = filter.data().capacity();
  for (auto i{0uz}; i < 2 * capacity + capacity / 2; ++i) {
    auto val = input[idx];
    filter.update(val);
    idx = (idx + 1) % input.size();
  }

  sum = value_type{0};
  for (auto i = 0uz; i < filter.data().size(); ++i) {
    sum += filter.data()[i];
  }
  assert(filter.output() == sum / static_cast<divisor_type>(capacity));

  filter.reset(value_type{-42});
  assert(filter.output() == value_type{-42});
  assert(filter.data().full());

  return true;
}

static_assert(test_moving_average_filter(emb::filters::moving_average<int, 1>{},
                                         0));
static_assert(test_moving_average_filter(emb::filters::moving_average<int, 2>{},
                                         0));
static_assert(test_moving_average_filter(emb::filters::moving_average<int, 5>{},
                                         0));
static_assert(
    test_moving_average_filter(emb::filters::moving_average<int, 10>{}, 0));

static_assert(
    test_moving_average_filter(emb::filters::moving_average<int, 1>{42}, 42));
static_assert(
    test_moving_average_filter(emb::filters::moving_average<int, 2>{42}, 42));
static_assert(
    test_moving_average_filter(emb::filters::moving_average<int, 5>{42}, 42));
static_assert(
    test_moving_average_filter(emb::filters::moving_average<int, 10>{42}, 42));

static_assert(test_moving_average_filter(
    emb::filters::moving_average<emb::units::erad_f32, 4>{},
    emb::units::erad_f32{0}));

// `reset` fills the window, whatever it held: the output moves away from the
// value as the copies leave the window, and the sum is exact once they have.
constexpr bool test_moving_average_reset()
{
  emb::filters::moving_average<float, 4> filter;
  filter.update(1.0f);
  filter.update(2.0f);

  filter.reset(10.0f);
  if (filter.output() != 10.0f || !filter.data().full()) return false;

  std::array const expected{7.5f, 5.0f, 2.5f, 0.0f, 0.0f};
  for (auto const output : expected) {
    filter.update(0.0f);
    if (filter.output() != output) return false;
  }
  return true;
}

static_assert(test_moving_average_reset());

// For an integer type the output after `reset` is rounded toward zero, as after
// any update.
constexpr bool test_moving_average_reset_int()
{
  emb::filters::moving_average<int, 2> filter;
  filter.reset(7);
  if (filter.output() != 7) return false;
  filter.update(0);
  if (filter.output() != 3) return false;
  filter.update(0);
  return filter.output() == 0;
}

static_assert(test_moving_average_reset_int());

// An initial output starts the filter settled on the value, as after `reset`;
// without one the window is empty, and the first input is the mean.
constexpr bool test_moving_average_start()
{
  emb::filters::moving_average<float, 4> settled{10.0f};
  if (settled.output() != 10.0f || !settled.data().full()) return false;
  settled.update(0.0f);
  if (settled.output() != 7.5f) return false;

  emb::filters::moving_average<float, 4> empty;
  if (empty.output() != 0.0f || !empty.data().empty()) return false;
  empty.update(8.0f);
  return empty.output() == 8.0f;
}

static_assert(test_moving_average_start());

// `full()` is false until `WindowSize` values are in, and true from the start
// with an initial output and once `reset` has filled the window.
constexpr bool test_moving_average_full()
{
  emb::filters::moving_average<float, 2> filter;
  if (filter.full()) return false;
  filter.update(1.0f);
  if (filter.full()) return false;
  filter.update(2.0f);
  if (!filter.full()) return false;

  emb::filters::moving_average<float, 2> settled{10.0f};
  if (!settled.full()) return false;

  emb::filters::moving_average<float, 2> after_reset;
  after_reset.reset(10.0f);
  return after_reset.full();
}

static_assert(test_moving_average_full());

// A value much larger than the rest no longer affects the output
// `WindowSize - 1` updates after it has left the window, wherever it falls
// among the inputs. An infinity or a NaN can't be tested here: constant
// evaluation rejects arithmetic that produces a NaN.
template<std::size_t WindowSize>
constexpr bool test_moving_average_outlier(float outlier)
{
  for (auto offset = 0uz; offset < WindowSize; ++offset) {
    emb::filters::moving_average<float, WindowSize> filter;
    for (auto i = 0uz; i < WindowSize + offset; ++i) {
      filter.update(1.0f);
    }
    filter.update(outlier);
    for (auto i = 0uz; i < 2 * WindowSize - 1; ++i) {
      filter.update(1.0f);
    }
    assert(filter.output() == 1.0f);
  }
  return true;
}

static_assert(test_moving_average_outlier<4>(1e8f));
static_assert(test_moving_average_outlier<16>(1e8f));

// `WindowSize` must be positive.
template<std::size_t WindowSize>
concept window_accepted =
    requires { typename emb::filters::moving_average<float, WindowSize>; };

static_assert(!window_accepted<0>);
static_assert(window_accepted<1>);

// By default the values are summed in the type of the sum of two `T`s: `int`
// for an integer `T` narrower than `int`, `T` itself otherwise.
static_assert(
    std::same_as<emb::filters::moving_average<std::int16_t, 4>::sum_type, int>);
static_assert(
    std::same_as<emb::filters::moving_average<std::uint8_t, 4>::sum_type, int>);
static_assert(
    std::same_as<emb::filters::moving_average<int, 4>::sum_type, int>);
static_assert(
    std::same_as<emb::filters::moving_average<float, 4>::sum_type, float>);
static_assert(std::same_as<
              emb::filters::moving_average<emb::units::erad_f32, 4>::sum_type,
              emb::units::erad_f32>);

// So the sum of the window may exceed `T`: sixteen 12-bit codes, or the
// extremes of `std::int16_t`. The output is still rounded toward zero.
constexpr bool test_moving_average_narrow_type()
{
  using filter = emb::filters::moving_average<std::int16_t, 16>;
  for (auto const value : std::array<std::int16_t, 3>{4095, 32767, -32768}) {
    filter f;
    for (auto i = 0uz; i < 2 * filter::window_size; ++i) {
      f.update(value);
    }
    if (f.output() != value) return false;
  }

  filter settled{30000};
  settled.update(0);
  if (settled.output() != 28125) return false;

  emb::filters::moving_average<std::int16_t, 4> mixed;
  mixed.update(32767);
  mixed.update(32767);
  mixed.update(-32768);
  mixed.update(1);
  return mixed.output() == 8191;
}

static_assert(test_moving_average_narrow_type());

// A floating-point `Sum` wider than `T`: once the outlier has left the window,
// a `double` sum has kept the inputs that came with it, a `float` sum has not.
template<typename Sum>
constexpr float moving_average_after_outlier()
{
  emb::filters::moving_average<float, 4, Sum> filter;
  for (auto i = 0uz; i < 4; ++i) {
    filter.update(1.0f);
  }
  filter.update(1e8f);
  for (auto i = 0uz; i < 4; ++i) {
    filter.update(1.0f);
  }
  return filter.output();
}

static_assert(moving_average_after_outlier<double>() == 1.0f);
static_assert(moving_average_after_outlier<float>() == 0.25f);

} // namespace
