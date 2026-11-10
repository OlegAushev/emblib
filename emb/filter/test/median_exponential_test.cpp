#include <emb/filter/concepts.hpp>
#include <emb/filter/exponential_filter.hpp>
#include <emb/filter/median_exponential_filter.hpp>
#include <emb/filter/median_filter.hpp>
#include <emb/units.hpp>

#include <array>
#include <concepts>

namespace {

using emb::units::eradps_f32;
using emb::units::sec_f32;

using float_filter = emb::median_exponential_filter<float, 3, sec_f32>;
using speed_filter = emb::median_exponential_filter<eradps_f32, 3, sec_f32>;

static_assert(emb::some_filter<float_filter>);
static_assert(emb::some_filter<speed_filter>);
static_assert(std::same_as<speed_filter::value_type, eradps_f32>);
static_assert(std::same_as<speed_filter::duration_type, sec_f32>);
static_assert(speed_filter::window_size == 3);

// Without an initial output, the output stays at zero until the window is full
// and then starts at its median rather than climbing from zero, wherever a lone
// outlier falls in the window.
consteval bool test_seed(std::array<float, 5> const& input)
{
  emb::median_exponential_filter<float, 5, sec_f32> filter{sec_f32{0.01f},
                                                           sec_f32{1.0f}};
  if (filter.output() != 0.0f) return false;
  for (auto i = 0uz; i + 1 < input.size(); ++i) {
    filter.step(input[i]);
    if (filter.output() != 0.0f) return false;
  }
  filter.step(input.back());
  return filter.output() == 20.0f;
}

static_assert(test_seed({1000.0f, 20.0f, 20.0f, 20.0f, 20.0f}));
static_assert(test_seed({20.0f, 20.0f, 1000.0f, 20.0f, 20.0f}));
static_assert(test_seed({20.0f, 20.0f, 20.0f, 20.0f, 1000.0f}));

// A window of one takes the first value as the seed.
consteval bool test_seed_of_one_value()
{
  emb::median_exponential_filter<float, 1, sec_f32> filter{sec_f32{0.5f},
                                                           sec_f32{1.0f}};
  filter.step(8.0f);
  if (filter.output() != 8.0f) return false;
  filter.step(10.0f);
  return filter.output() == 9.0f;
}

static_assert(test_seed_of_one_value());

// Past the seed, the output is that of the same two stages driven by hand.
consteval bool test_after_seed()
{
  emb::median_filter<float, 3> median;
  emb::exponential_filter<float, sec_f32> smoother{sec_f32{0.01f},
                                                   sec_f32{0.1f}};
  float_filter filter{sec_f32{0.01f}, sec_f32{0.1f}};
  std::array const input{10.0f, 20.0f, 90.0f, -80.0f, 30.0f};
  for (auto i = 0uz; i < 40; ++i) {
    median.push(input[i % input.size()]);
    if (i == 2) {
      smoother.reset(median.output());
    }
    else if (i > 2) {
      smoother.step(median.output());
    }
    filter.step(input[i % input.size()]);
    if (filter.output() != smoother.output()) return false;
  }
  return true;
}

static_assert(test_after_seed());

// With an initial output, the filter starts settled at it: the output is that
// of the same two stages driven by hand from the first step, also once it is
// retuned.
consteval bool test_initial_output()
{
  eradps_f32 const start{5};
  emb::median_filter<eradps_f32, 3> median{start};
  emb::exponential_filter<eradps_f32, sec_f32> smoother{sec_f32{0.01f},
                                                        sec_f32{0.1f},
                                                        start};
  speed_filter filter{sec_f32{0.01f}, sec_f32{0.1f}, start};
  if (filter.output() != start) return false;
  std::array const input{eradps_f32{10},
                         eradps_f32{90},
                         eradps_f32{20},
                         eradps_f32{-80},
                         eradps_f32{30}};
  for (auto i = 0uz; i < 40; ++i) {
    if (i == 30) {
      smoother.set_time_constant(sec_f32{0.3f});
      filter.set_time_constant(sec_f32{0.3f});
    }
    median.push(input[i % input.size()]);
    smoother.step(median.output());
    filter.step(input[i % input.size()]);
    if (filter.output() != smoother.output()) return false;
  }
  return true;
}

static_assert(test_initial_output());

// `reset` settles both stages on the value and ends the wait for the window:
// the median holds the value for the next step, and the exponential stage takes
// the next one at once.
consteval bool test_reset()
{
  float_filter filter{sec_f32{0.5f}, sec_f32{1.0f}};
  filter.step(100.0f);

  filter.reset(10.0f);
  if (filter.output() != 10.0f) return false;
  filter.step(30.0f);
  if (filter.output() != 10.0f) return false;
  filter.step(30.0f);
  return filter.output() == 20.0f;
}

static_assert(test_reset());

} // namespace
