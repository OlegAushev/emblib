#ifdef __cpp_constexpr

#include <emb/filter.hpp>
#include <emb/units.hpp>

#include <array>

namespace emb {
namespace tests {

constexpr bool test_median_filter(
    emb::filter::median_filter_type auto filter,
    typename decltype(filter)::value_type init_output
) {
  using value_type = decltype(filter)::value_type;
  constexpr size_t window_size = decltype(filter)::window_size;

  assert(filter.output() == init_output);

  filter.set_output(value_type{-42});
  assert(filter.output() == value_type{-42});

  filter.reset();
  assert(filter.output() == init_output);

  // Test with known sequence
  std::array<value_type, 7> input{
      value_type{10},
      value_type{90},
      value_type{20},
      value_type{80},
      value_type{30},
      value_type{70},
      value_type{40}
  };

  // Fill window with first value
  for (size_t i = 0; i < window_size; ++i) {
    filter.push(input[0]);
  }
  assert(filter.output() == input[0]);

  // Push varying values
  for (size_t i = 0; i < input.size(); ++i) {
    filter.push(input[i]);
  }

  // Output should be within the range of input values
  value_type const output = filter.output();
  value_type min_val = input[0];
  value_type max_val = input[0];
  for (auto const& val : input) {
    if (val < min_val) min_val = val;
    if (val > max_val) max_val = val;
  }
  assert(output >= min_val && output <= max_val);

  filter.reset();
  assert(filter.output() == init_output);

  return true;
}

// Test with int and various odd window sizes
static_assert(test_median_filter(emb::filter::median<int, 1>{}, 0));
static_assert(test_median_filter(emb::filter::median<int, 3>{}, 0));
static_assert(test_median_filter(emb::filter::median<int, 5>{}, 0));
static_assert(test_median_filter(emb::filter::median<int, 7>{}, 0));

static_assert(test_median_filter(emb::filter::median<int, 1>{42}, 42));
static_assert(test_median_filter(emb::filter::median<int, 3>{42}, 42));
static_assert(test_median_filter(emb::filter::median<int, 5>{42}, 42));

// Test with float
static_assert(test_median_filter(emb::filter::median<float, 3>{}, 0.0f));
static_assert(test_median_filter(emb::filter::median<float, 5>{}, 0.0f));
static_assert(test_median_filter(emb::filter::median<float, 3>{3.14f}, 3.14f));

// Test with units
static_assert(test_median_filter(
    emb::filter::median<emb::units::erad_f32, 3>{},
    emb::units::erad_f32{0}
));

} // namespace tests
} // namespace emb

#endif
