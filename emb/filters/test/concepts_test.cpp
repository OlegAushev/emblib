#include <emb/filters/concepts.hpp>
#include <emb/filters/exponential.hpp>
#include <emb/filters/median.hpp>
#include <emb/filters/median_exponential.hpp>
#include <emb/filters/moving_average.hpp>
#include <emb/filters/passthrough.hpp>
#include <emb/integrator.hpp>
#include <emb/units.hpp>

namespace {

using emb::units::sec_f32;

static_assert(emb::filters::filter<emb::filters::passthrough<float>>);
static_assert(emb::filters::filter<emb::filters::median<float, 3>>);
static_assert(emb::filters::filter<emb::filters::moving_average<float, 4>>);
static_assert(emb::filters::filter<emb::filters::exponential<float, sec_f32>>);
static_assert(
    emb::filters::filter<emb::filters::median_exponential<float, 3, sec_f32>>);

// `integrator` has a `value_type`, an `output` and a `step` that takes a
// `value_type`, and is not a filter: only the name `update` sets one apart.
static_assert(!emb::filters::filter<emb::integrator<float>>);
static_assert(!emb::filters::filter<emb::filters::median<float, 3>&>);

} // namespace
