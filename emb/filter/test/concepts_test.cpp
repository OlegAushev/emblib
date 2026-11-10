#include <emb/filter/concepts.hpp>
#include <emb/filter/exponential_filter.hpp>
#include <emb/filter/median_exponential_filter.hpp>
#include <emb/filter/median_filter.hpp>
#include <emb/filter/moving_average_filter.hpp>
#include <emb/filter/passthrough_filter.hpp>
#include <emb/integrator.hpp>
#include <emb/units.hpp>

namespace {

using emb::units::sec_f32;

static_assert(emb::some_filter<emb::passthrough_filter<float>>);
static_assert(emb::some_filter<emb::median_filter<float, 3>>);
static_assert(emb::some_filter<emb::moving_average_filter<float, 4>>);
static_assert(emb::some_filter<emb::exponential_filter<float, sec_f32>>);
static_assert(
    emb::some_filter<emb::median_exponential_filter<float, 3, sec_f32>>);

// A type is a filter only if it declares the hooks: `integrator` has a
// `value_type`, a `step` and an `output` and is not one.
static_assert(!emb::some_filter<emb::integrator<float>>);
static_assert(!emb::some_filter<emb::median_filter<float, 3>&>);

// The hooks call the members: each filter below, fed one value, outputs it.
consteval bool test_hooks()
{
  emb::passthrough_filter<float> passthrough;
  emb::median_filter<float, 1> median;
  emb::moving_average_filter<float, 1> average;
  emb::exponential_filter<float, sec_f32> smoother{sec_f32{1.0f},
                                                   sec_f32{1.0f}};
  emb::median_exponential_filter<float, 1, sec_f32> both{sec_f32{1.0f},
                                                         sec_f32{1.0f}};
  filter_feed(passthrough, 1.0f);
  filter_feed(median, 2.0f);
  filter_feed(average, 3.0f);
  filter_feed(smoother, 4.0f);
  filter_feed(both, 5.0f);
  return filter_output(passthrough) == 1.0f
      && filter_output(median) == 2.0f
      && filter_output(average) == 3.0f
      && filter_output(smoother) == 4.0f
      && filter_output(both) == 5.0f;
}

static_assert(test_hooks());

} // namespace
