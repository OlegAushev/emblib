#include <emb/sensor/validated.hpp>

#include <emb/concurrent/spsc_queue.hpp>
#include <emb/filters/concepts.hpp>
#include <emb/filters/exponential.hpp>
#include <emb/filters/passthrough.hpp>
#include <emb/sensor/buffered.hpp>
#include <emb/sensor/concepts.hpp>
#include <emb/sensor/multichannel.hpp>
#include <emb/sensor/multiplexed.hpp>
#include <emb/sensor/signalpath.hpp>
#include <emb/sensor/singlechannel.hpp>
#include <emb/sensor/within.hpp>
#include <emb/units.hpp>

#include <array>
#include <concepts>
#include <cstdint>
#include <expected>
#include <optional>

namespace {

using namespace emb::units;
using emb::sensor::fault;

// a 12-bit converter over a 3.3 V reference, read as the pin voltage
struct adc {
  static constexpr std::uint16_t forward(volt_f32 v)
  {
    return static_cast<std::uint16_t>(v.value / 3.3f * 4095.f);
  }

  static constexpr volt_f32 inverse(std::uint16_t code)
  {
    return volt_f32{code * 3.3f / 4095.f};
  }
};

using path = emb::sensor::signalpath<adc>;
using core = emb::sensor::singlechannel<std::uint16_t,
                                        path,
                                        emb::filters::passthrough<volt_f32>>;

constexpr volt_f32 reading(std::uint16_t code)
{
  return path{}.inverse(code);
}

// the codes of a healthy chain: a shorted or an open sensor drives the pin
// towards a rail, out of them
inline constexpr emb::sensor::bounds<std::uint16_t> healthy{100, 3900};
using guard = emb::sensor::within<healthy>;
using channel = emb::sensor::validated<guard, core>;

inline constexpr std::uint16_t healthy_code{2000};
inline constexpr std::uint16_t shorted_code{50};
inline constexpr std::uint16_t open_code{4000};

static_assert(emb::sensor::some_immediate_sensor<channel>);
static_assert(emb::sensor::some_singlechannel_sensor<channel>);
static_assert(std::same_as<channel::core_type, core>);
static_assert(std::same_as<channel::input_type, std::uint16_t>);
static_assert(
    std::same_as<channel::output_type, std::expected<volt_f32, fault>>);

// like the sensor it wraps, it is no filter
static_assert(!emb::filters::filter<channel>);

// an output has no order, so a check against a threshold takes the value out
// first, where an optional would answer the check without a value
template<typename T, typename U>
constexpr bool orders_against = requires(T t, U u) { t < u; };

static_assert(!orders_against<channel::output_type, volt_f32>);
static_assert(!std::totally_ordered<channel::output_type>);
static_assert(orders_against<std::optional<volt_f32>, volt_f32>);

constexpr bool test_no_output_before_the_first_sample()
{
  using smoothing = emb::filters::exponential<volt_f32, sec_f32>;
  using smoothed = emb::sensor::singlechannel<std::uint16_t, path, smoothing>;

  // even a filter that starts at an initial output has read nothing yet
  emb::sensor::validated<guard, smoothed> const c{
      path{},
      smoothing{sec_f32{0.001f}, sec_f32{0.01f}, volt_f32{1.f}}};
  return c.output() == std::unexpected(fault::no_sample)
      && c.core().output() == volt_f32{1.f};
}

constexpr bool test_refused_sample_stops_short_of_the_core()
{
  channel c;

  c.update(healthy_code);
  if (c.output() != reading(healthy_code)) return false;

  // a shorted sensor, then an open one: neither reaches the core
  c.update(shorted_code);
  if (c.output() != std::unexpected(fault::below)) return false;
  if (c.core().output() != reading(healthy_code)) return false;

  c.update(open_code);
  if (c.output() != std::unexpected(fault::above)) return false;
  if (c.core().output() != reading(healthy_code)) return false;

  // and the first sample back within the bounds is read again
  c.update(std::uint16_t{1500});
  return c.output() == reading(1500);
}

// To the filter of the core, a refused sample is one never taken: it follows
// the same filter fed the accepted samples alone.
constexpr bool test_filter_sees_accepted_samples_alone()
{
  using smoothing = emb::filters::exponential<volt_f32, sec_f32>;
  using smoothed = emb::sensor::singlechannel<std::uint16_t, path, smoothing>;

  smoothing const start{sec_f32{0.25f}, sec_f32{1.f}};
  emb::sensor::validated<guard, smoothed> c{path{}, start};
  smoothing alone = start;

  for (std::uint16_t const code : {std::uint16_t{1000},
                                   open_code,
                                   open_code,
                                   std::uint16_t{3000},
                                   shorted_code,
                                   healthy_code}) {
    c.update(code);
    if (guard{}(code) == std::nullopt) {
      alone.update(reading(code));
    }
    if (c.core().output() != alone.output()) return false;
  }
  return true;
}

// a validator with bounds of its own, which can change at run time
struct adjustable {
  emb::sensor::bounds<std::uint16_t> bounds;

  constexpr std::optional<fault> operator()(std::uint16_t code) const
  {
    if (code < bounds.min) return fault::below;
    if (bounds.max < code) return fault::above;
    return std::nullopt;
  }
};

constexpr bool test_bounds_held_by_the_validator()
{
  emb::sensor::validated<adjustable, core> c{adjustable{{100, 200}}, core{}};

  c.update(std::uint16_t{300});
  if (c.output() != std::unexpected(fault::above)) return false;

  c.validator().bounds.max = 400;
  c.update(std::uint16_t{300});
  return c.output() == reading(300);
}

// the stages of the core are reached through validated as through the core
template<typename S>
constexpr bool has_converter = requires(S s) { s.converter(); };
template<typename S>
constexpr bool has_filter = requires(S s) { s.filter(); };

static_assert(has_converter<channel> && has_filter<channel>);
static_assert(has_converter<channel const> && has_filter<channel const>);

constexpr bool test_stages_of_the_core()
{
  channel c;
  channel const& view = c;
  return &c.converter() == &c.core().converter()
      && &c.filter() == &c.core().filter()
      && &view.converter() == &view.core().converter()
      && &view.filter() == &view.core().filter();
}

// any immediate single-channel sensor serves as a core, and one without
// stages to reach leaves validated without them too
struct counter {
  using input_type = std::uint16_t;
  using output_type = int;
  using sensor_category = emb::sensor::immediate_tag;

  int count = 0;

  constexpr void update(input_type)
  {
    ++count;
  }

  constexpr output_type output() const
  {
    return count;
  }
};

static_assert(!has_converter<emb::sensor::validated<guard, counter>>);
static_assert(!has_filter<emb::sensor::validated<guard, counter>>);

constexpr bool test_core_counts_accepted_samples()
{
  emb::sensor::validated<guard, counter> c;
  for (std::uint16_t const code :
       {shorted_code, healthy_code, open_code, std::uint16_t{1500}}) {
    c.update(code);
  }
  return c.output() == 2;
}

// a frame takes a frame of samples and gives a frame of outputs
using frame_of_four = emb::sensor::multichannel_n<channel, 4>;

static_assert(
    std::same_as<frame_of_four::input_type, std::array<std::uint16_t, 4>>);
static_assert(std::same_as<frame_of_four::output_type,
                           std::array<std::expected<volt_f32, fault>, 4>>);

// in a frame, each channel judges its own sample of it
constexpr bool test_channels_of_a_frame()
{
  emb::sensor::multichannel_n<channel, 4> frame;
  frame.update({shorted_code, healthy_code, open_code, std::uint16_t{1500}});

  auto const outputs = frame.outputs();
  return outputs[0] == std::unexpected(fault::below)
      && outputs[1] == reading(healthy_code)
      && outputs[2] == std::unexpected(fault::above)
      && outputs[3] == reading(1500)
      && frame.output(2uz) == std::unexpected(fault::above);
}

// a multiplexer takes the sample of one source and gives the outputs of all
using three_sources = emb::sensor::multiplexed_n<channel, 3>;

static_assert(std::same_as<three_sources::input_type, std::uint16_t>);
static_assert(std::same_as<three_sources::output_type,
                           std::array<std::expected<volt_f32, fault>, 3>>);

// behind a multiplexer, each source judges its own samples, and one not
// selected yet has read nothing
constexpr bool test_sources_of_a_multiplexer()
{
  emb::sensor::multiplexed_n<channel, 3> sources;
  sources.update(0uz, healthy_code);
  sources.update(1uz, open_code);
  return sources.output(0uz) == reading(healthy_code)
      && sources.output(1uz) == std::unexpected(fault::above)
      && sources.output(2uz) == std::unexpected(fault::no_sample)
      && &sources.sensor(1uz).filter() == &sources.sensor(1uz).core().filter();
}

static_assert(test_no_output_before_the_first_sample());
static_assert(test_refused_sample_stops_short_of_the_core());
static_assert(test_filter_sees_accepted_samples_alone());
static_assert(test_bounds_held_by_the_validator());
static_assert(test_stages_of_the_core());
static_assert(test_core_counts_accepted_samples());
static_assert(test_channels_of_a_frame());
static_assert(test_sources_of_a_multiplexer());

// queued, the samples are judged as update() drains them, where they are
// converted
using queue = emb::spsc_queue<std::uint16_t, 4>;
using queued = emb::sensor::buffered<queue, channel>;

static_assert(emb::sensor::some_deferred_sensor<queued>);
static_assert(
    std::same_as<queued::output_type, std::expected<volt_f32, fault>>);
static_assert(emb::sensor::some_deferred_sensor<
              emb::sensor::multiplexed_n<queued, 3>::sensor_type<0>>);

// It wraps the sensor that converts and nothing around it: a buffered one
// would have it judge a sample when it is parked, and neither a frame nor a
// multiplexer is a single channel.
template<typename V, typename C>
constexpr bool wraps = requires { typename emb::sensor::validated<V, C>; };

static_assert(wraps<guard, core>);
static_assert(!wraps<guard, emb::sensor::buffered<queue, core>>);
static_assert(!wraps<guard, emb::sensor::multichannel_n<core, 2>>);
static_assert(!wraps<guard, emb::sensor::multiplexed_n<core, 2>>);

// A validator returns std::optional<fault> itself: one returning a bare fault,
// which converts to one, would find a fault in every sample. It is called as
// const, so one that would keep a history of the samples is refused too.
struct bare_fault {
  constexpr fault operator()(std::uint16_t) const
  {
    return fault::below;
  }
};

struct with_history {
  int seen = 0;

  constexpr std::optional<fault> operator()(std::uint16_t)
  {
    ++seen;
    return std::nullopt;
  }
};

static_assert(wraps<adjustable, core>);
static_assert(!wraps<bare_fault, core>);
static_assert(!wraps<with_history, core>);

// a frame takes channels of one output type, so it does not mix channels
// that judge their samples with channels that do not
template<typename... Cores>
constexpr bool frames =
    requires { typename emb::sensor::multichannel<Cores...>; };

static_assert(frames<channel, channel>);
static_assert(!frames<channel, core>);

} // namespace
