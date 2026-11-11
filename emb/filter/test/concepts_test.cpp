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

// `integrator` has a `value_type`, an `output` and a `step` that takes a
// `value_type`, and is not a filter: only the name `update` sets one apart.
static_assert(!emb::some_filter<emb::integrator<float>>);
static_assert(!emb::some_filter<emb::median_filter<float, 3>&>);

} // namespace
