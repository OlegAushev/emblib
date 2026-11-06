#include <emb/filter/median_filter.hpp>
#include <emb/units.hpp>

#include <array>
#include <cassert>

namespace {

template<typename Filter>
constexpr bool test_median_filter(Filter filter,
                                  typename Filter::value_type init_output)
{
  using value_type = Filter::value_type;
  constexpr std::size_t window_size = decltype(filter)::window_size;

  assert(filter.output() == init_output);

  filter.reset(value_type{-42});
  assert(filter.output() == value_type{-42});

  filter.reset(init_output);
  assert(filter.output() == init_output);

  // Test with known sequence
  std::array<value_type, 7> input{value_type{10},
                                  value_type{90},
                                  value_type{20},
                                  value_type{80},
                                  value_type{30},
                                  value_type{70},
                                  value_type{40}};

  // Replace the window contents with the first value
  for (auto i = 0uz; i < window_size; ++i) {
    filter.push(input[0]);
  }
  assert(filter.output() == input[0]);

  // Push varying values
  for (auto i = 0uz; i < input.size(); ++i) {
    filter.push(input[i]);
  }

  // Output should be within the range of input values
  [[maybe_unused]] value_type const output = filter.output();
  value_type min_val = input[0];
  value_type max_val = input[0];
  for (auto const& val : input) {
    if (val < min_val) min_val = val;
    if (val > max_val) max_val = val;
  }
  assert(output >= min_val && output <= max_val);

  filter.reset(init_output);
  assert(filter.output() == init_output);

  return true;
}

// Test with int and various odd window sizes
static_assert(test_median_filter(emb::median_filter<int, 1>{}, 0));
static_assert(test_median_filter(emb::median_filter<int, 3>{}, 0));
static_assert(test_median_filter(emb::median_filter<int, 5>{}, 0));
static_assert(test_median_filter(emb::median_filter<int, 7>{}, 0));

static_assert(test_median_filter(emb::median_filter<int, 1>{42}, 42));
static_assert(test_median_filter(emb::median_filter<int, 3>{42}, 42));
static_assert(test_median_filter(emb::median_filter<int, 5>{42}, 42));

// Test with float
static_assert(test_median_filter(emb::median_filter<float, 3>{}, 0.0f));
static_assert(test_median_filter(emb::median_filter<float, 5>{}, 0.0f));
static_assert(test_median_filter(emb::median_filter<float, 3>{3.14f}, 3.14f));

// Test with units
static_assert(test_median_filter(emb::median_filter<emb::units::erad_f32, 3>{},
                                 emb::units::erad_f32{0}));

// Without an initial output, the output stays at zero until the window is full
// and then starts at its median, wherever a lone outlier falls in the window.
consteval bool test_wait(std::array<float, 5> const& input)
{
  emb::median_filter<float, 5> filter;
  if (filter.output() != 0.0f || filter.full()) return false;
  for (auto i = 0uz; i + 1 < input.size(); ++i) {
    filter.push(input[i]);
    if (filter.output() != 0.0f || filter.full()) return false;
  }
  filter.push(input.back());
  return filter.output() == 20.0f && filter.full();
}

static_assert(test_wait({1000.0f, 20.0f, 20.0f, 20.0f, 20.0f}));
static_assert(test_wait({20.0f, 20.0f, 1000.0f, 20.0f, 20.0f}));
static_assert(test_wait({20.0f, 20.0f, 20.0f, 20.0f, 1000.0f}));

// A window of one is full with the first value.
consteval bool test_wait_of_one_value()
{
  emb::median_filter<float, 1> filter;
  if (filter.full()) return false;
  filter.push(8.0f);
  return filter.output() == 8.0f && filter.full();
}

static_assert(test_wait_of_one_value());

// Zero as the initial output is not the same as none: its copies count as
// values pushed, so the output moves away from it while the other still waits.
consteval bool test_zero_is_not_none()
{
  emb::median_filter<float, 3> zero{0.0f};
  emb::median_filter<float, 3> none;
  if (!zero.full() || none.full()) return false;
  for (auto input : std::array{10.0f, 20.0f}) {
    zero.push(input);
    none.push(input);
  }
  return zero.output() == 10.0f && none.output() == 0.0f;
}

static_assert(test_zero_is_not_none());

// `reset` ends the wait: the value holds for the next push, and then the two
// values pushed outnumber its copies.
consteval bool test_reset_in_wait()
{
  emb::median_filter<float, 3> filter;
  filter.push(1.0f);
  filter.reset(5.0f);
  if (filter.output() != 5.0f || !filter.full()) return false;
  filter.push(9.0f);
  if (filter.output() != 5.0f) return false;
  filter.push(9.0f);
  return filter.output() == 9.0f;
}

static_assert(test_reset_in_wait());

} // namespace
