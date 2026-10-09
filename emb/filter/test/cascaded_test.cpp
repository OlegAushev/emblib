#include <emb/filter/cascaded_filter.hpp>
#include <emb/filter/concepts.hpp>
#include <emb/filter/exponential_filter.hpp>
#include <emb/filter/median_filter.hpp>
#include <emb/filter/moving_average_filter.hpp>
#include <emb/filter/passthrough_filter.hpp>
#include <emb/units.hpp>

#include <array>
#include <concepts>
#include <tuple>

namespace {

using emb::units::eradps_f32;
using emb::units::sec_f32;

template<typename... Stages>
concept cascadable = requires { typename emb::cascaded_filter<Stages...>; };

template<typename F>
concept has_set_output =
    requires(F f, typename F::value_type v) { f.set_output(v); };

template<typename F>
concept has_reset = requires(F f) { f.reset(); };

using median_into_average =
    emb::cascaded_filter<emb::median_filter<float, 3>,
                         emb::moving_average_filter<float, 4>>;
using passthrough_into_median =
    emb::cascaded_filter<emb::passthrough_filter<float>,
                         emb::median_filter<float, 3>>;

// At least one stage, all of one value type, which the cascade takes as its
// own; the cascade is a filter itself.
static_assert(!cascadable<>);
static_assert(!cascadable<float>);
static_assert(!cascadable<emb::median_filter<int, 3>,
                          emb::moving_average_filter<float, 4>>);
static_assert(cascadable<emb::median_filter<float, 3>>);
static_assert(std::same_as<median_into_average::value_type, float>);
static_assert(emb::some_filter<median_into_average>);

// `set_output` and `reset` exist only if every stage has them.
static_assert(has_set_output<median_into_average>);
static_assert(has_reset<median_into_average>);
static_assert(!has_set_output<passthrough_into_median>);
static_assert(!has_reset<passthrough_into_median>);

// Three stages, each output worked out by hand: the median gives 0 10 10 40
// 70, the first average 0 5 10 25 55, and the second, rounding toward zero,
// 0 2 7 17 40.
consteval bool test_three_stages()
{
  emb::cascaded_filter<emb::median_filter<int, 3>,
                       emb::moving_average_filter<int, 2>,
                       emb::moving_average_filter<int, 2>>
      cascade;
  std::array const input{10, 40, 10, 70, 70};
  std::array const expected{0, 2, 7, 17, 40};
  for (auto i = 0uz; i < input.size(); ++i) {
    cascade.push(input[i]);
    if (cascade.output() != expected[i]) return false;
  }
  return true;
}

static_assert(test_three_stages());

// A median feeding an exponential filter, as for the speed of
// `hall::angle_sensor`: the output is that of the same two stages driven by
// hand, before the first push and after every push, also once a stage is
// retuned through `stages`.
consteval bool test_median_into_exponential()
{
  emb::median_filter<eradps_f32, 3> median;
  emb::exponential_filter<eradps_f32, sec_f32> smoother(sec_f32{0.01f},
                                                        sec_f32{0.1f});
  emb::cascaded_filter cascade{median, smoother};
  std::array const input{eradps_f32{10},
                         eradps_f32{90},
                         eradps_f32{20},
                         eradps_f32{-80},
                         eradps_f32{30}};
  if (cascade.output() != smoother.output()) return false;
  for (auto i = 0uz; i < 40; ++i) {
    if (i == 20) {
      smoother.set_timestep(sec_f32{0.05f});
      std::get<1>(cascade.stages).set_timestep(sec_f32{0.05f});
    }
    median.push(input[i % input.size()]);
    smoother.push(median.output());
    cascade.push(input[i % input.size()]);
    if (cascade.output() != smoother.output()) return false;
  }
  return true;
}

static_assert(test_median_into_exponential());

// A cascade as a stage of another one gives the output of one cascade of all
// the stages.
consteval bool test_nested()
{
  using median = emb::median_filter<int, 3>;
  using average = emb::moving_average_filter<int, 2>;
  emb::cascaded_filter<emb::cascaded_filter<median, average>, average> nested;
  emb::cascaded_filter<median, average, average> flat;
  std::array const input{10, 40, 10, 70, 70, -20, 5};
  for (auto x : input) {
    nested.push(x);
    flat.push(x);
    if (nested.output() != flat.output()) return false;
  }
  return true;
}

static_assert(test_nested());

// `set_output` gives every stage the value, so the median's window fills with
// it and holds it for the next push; `reset` returns every stage to its own
// initial output.
consteval bool test_set_output_and_reset()
{
  emb::cascaded_filter cascade{emb::median_filter<float, 3>{1.0f},
                               emb::moving_average_filter<float, 4>{2.0f}};
  auto const& [median, average] = cascade.stages;
  if (cascade.output() != 2.0f) return false;
  cascade.push(5.0f);
  cascade.push(6.0f);

  cascade.set_output(-3.0f);
  if (median.output() != -3.0f || average.output() != -3.0f) return false;
  if (cascade.output() != -3.0f) return false;
  cascade.push(10.0f);
  if (cascade.output() != -3.0f) return false;

  cascade.reset();
  return median.output() == 1.0f
      && average.output() == 2.0f
      && cascade.output() == 2.0f;
}

static_assert(test_set_output_and_reset());

} // namespace
