#include <emb/sensor/signalpath.hpp>

#include <emb/filter/passthrough_filter.hpp>
#include <emb/math.hpp>
#include <emb/sensor/concepts.hpp>
#include <emb/sensor/singlechannel.hpp>
#include <emb/signal/bind.hpp>
#include <emb/signal/proportional.hpp>

#include <cstdint>

namespace {

using namespace emb::units;

// one channel's worth of physical path: a transducer feeding a 12-bit
// converter over a 3.3 V reference
inline constexpr emb::signal::proportional transducer{amp_f32{600.f},
                                                      volt_f32{4.f}};

struct adc {
  static constexpr std::uint16_t forward(volt_f32 v)
  {
    return static_cast<std::uint16_t>(v.value() / 3.3f * 4095.f);
  }

  static constexpr volt_f32 inverse(std::uint16_t code)
  {
    return volt_f32{code * 3.3f / 4095.f};
  }
};

// the head of a path is the one stage that may carry run-time state
struct zero_trim {
  amp_f32 offset{0.f};

  constexpr amp_f32 forward(amp_f32 i) const
  {
    return i + offset;
  }

  constexpr amp_f32 inverse(amp_f32 i) const
  {
    return i - offset;
  }
};

using channel_path = emb::sensor::signalpath<emb::signal::bind<transducer>, adc>;

// a sensor takes it as it is, with nothing wrapped around it
static_assert(emb::sensor::some_converter<channel_path, std::uint16_t, amp_f32>);

constexpr bool test_call_is_the_measuring_direction()
{
  constexpr channel_path channel;

  // calling means inverse, and the path underneath still says so out loud
  assert(emb::approx(channel(std::uint16_t{2048}),
                     channel.inverse(std::uint16_t{2048}),
                     amp_f32{1e-6f}));

  // while the driving direction stays reachable by name
  assert(channel.forward(amp_f32{0.f}) == std::uint16_t{0});
  assert(emb::approx(channel.inverse(channel.forward(amp_f32{450.f})),
                     amp_f32{450.f},
                     amp_f32{0.5f}));

  return true;
}

constexpr bool test_drives_a_channel()
{
  emb::sensor::singlechannel<std::uint16_t,
                             channel_path,
                             emb::passthrough_filter<amp_f32>>
      channel;

  constexpr auto code = channel_path{}.forward(amp_f32{300.f});
  channel.submit(code);
  assert(emb::approx(channel.value(), amp_f32{300.f}, amp_f32{0.5f}));

  return true;
}

constexpr bool test_stateful_head()
{
  // stages are handed in by value, exactly as the path underneath takes them
  emb::sensor::signalpath<zero_trim, emb::signal::bind<transducer>, adc> const
      trimmed{zero_trim{amp_f32{5.f}}, {}, {}};

  constexpr emb::sensor::signalpath<zero_trim, emb::signal::bind<transducer>, adc>
      untrimmed;

  assert(trimmed.forward(amp_f32{0.f}) == untrimmed.forward(amp_f32{5.f}));
  assert(emb::approx(trimmed(untrimmed.forward(amp_f32{5.f})),
                     amp_f32{0.f},
                     amp_f32{0.5f}));

  return true;
}

static_assert(test_call_is_the_measuring_direction());
static_assert(test_drives_a_channel());
static_assert(test_stateful_head());

} // namespace
