#ifdef __cpp_constexpr

#include <emb/filter.hpp>
#include <emb/units.hpp>

namespace emb {
namespace tests {

constexpr bool test_exponential_filter(
    emb::filter::exponential_filter_type auto filter,
    typename decltype(filter)::value_type init_output
) {
  using value_type = decltype(filter)::value_type;

  assert(filter.output() == init_output);

  filter.set_output(value_type{100});
  assert(filter.output() == value_type{100});

  filter.reset();
  assert(filter.output() == init_output);

  // Test with step input - output should approach input value
  value_type const input_value{50};
  value_type prev_output = filter.output();

  // Push same value multiple times
  for (int i = 0; i < 20; ++i) {
    filter.push(input_value);
    value_type const curr_output = filter.output();

    // Output should monotonically approach input
    if (prev_output < input_value) {
      assert(curr_output >= prev_output);
    } else if (prev_output > input_value) {
      assert(curr_output <= prev_output);
    }

    prev_output = curr_output;
  }

  // After many iterations, should be close to input
  assert(filter.output() != init_output);

  // Test smooth_factor is in valid range [0, 1]
  [[maybe_unused]] auto const factor = filter.smooth_factor();
  assert(factor >= decltype(factor)(0));
  assert(factor <= decltype(factor)(1));

  filter.reset();
  assert(filter.output() == init_output);

  return true;
}

// Test with float
static_assert(test_exponential_filter(
    emb::filter::exponential<float, emb::units::sec_f32>(
        emb::units::sec_f32{0.01f},
        emb::units::sec_f32{0.1f}
    ),
    0.0f
));

static_assert(test_exponential_filter(
    emb::filter::exponential<float, emb::units::sec_f32>(
        emb::units::sec_f32{0.01f},
        emb::units::sec_f32{0.1f},
        3.14f
    ),
    3.14f
));

// Test with units
static_assert(test_exponential_filter(
    emb::filter::exponential<emb::units::erad_f32, emb::units::sec_f32>(
        emb::units::sec_f32{0.01f},
        emb::units::sec_f32{0.1f}
    ),
    emb::units::erad_f32{0}
));

} // namespace tests
} // namespace emb

#endif
