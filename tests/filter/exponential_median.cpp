#ifdef __cpp_constexpr

#include <emb/filter.hpp>
#include <emb/units.hpp>

#include <array>

namespace emb {
namespace tests {

constexpr bool test_exponential_median_filter(
    emb::filter::exponential_median_filter_type auto filter,
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

  // Push values - combines median filtering with exponential smoothing
  for (size_t i = 0; i < window_size * 3; ++i) {
    filter.push(input[i % input.size()]);
  }

  // Output should be within range of input values
  [[maybe_unused]] value_type const output = filter.output();
  value_type min_val = input[0];
  value_type max_val = input[0];
  for (auto const& val : input) {
    if (val < min_val) min_val = val;
    if (val > max_val) max_val = val;
  }
  assert(output >= min_val && output <= max_val);

  // Test smooth_factor is in valid range [0, 1]
  [[maybe_unused]] auto const factor = filter.smooth_factor();
  assert(factor >= decltype(factor)(0));
  assert(factor <= decltype(factor)(1));

  filter.reset();
  assert(filter.output() == init_output);

  return true;
}

// Test with float
static_assert(test_exponential_median_filter(
    emb::filter::exponential_median<float, 3, emb::units::sec_f32>(
        emb::units::sec_f32{0.01f},
        emb::units::sec_f32{0.1f}
    ),
    0.0f
));

static_assert(test_exponential_median_filter(
    emb::filter::exponential_median<float, 5, emb::units::sec_f32>(
        emb::units::sec_f32{0.01f},
        emb::units::sec_f32{0.1f}
    ),
    0.0f
));

static_assert(test_exponential_median_filter(
    emb::filter::exponential_median<float, 3, emb::units::sec_f32>(
        emb::units::sec_f32{0.01f},
        emb::units::sec_f32{0.1f},
        3.14f
    ),
    3.14f
));

// Test with units
static_assert(test_exponential_median_filter(
    emb::filter::exponential_median<emb::units::erad_f32, 3, emb::units::sec_f32>(
        emb::units::sec_f32{0.01f},
        emb::units::sec_f32{0.1f}
    ),
    emb::units::erad_f32{0}
));

} // namespace tests
} // namespace emb

#endif
