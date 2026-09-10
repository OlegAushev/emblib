#include <emb/signal/path.hpp>

#include <emb/math.hpp>
#include <emb/signal/bind.hpp>
#include <emb/signal/proportional.hpp>

#include <cstdint>
#include <type_traits>

namespace {

using namespace emb::units;
using emb::signal::bind;
using emb::signal::path;

// ---- stages standing in for one phase-current channel ----

// the transducer, a constexpr object rather than a type: +/-4 V at +/-600 A
inline constexpr emb::signal::proportional transducer{amp_f32{600.f},
                                                      volt_f32{4.f}};

// the board around it: half the swing, lifted to mid-rail
struct conditioning {
  static constexpr volt_f32 forward(volt_f32 v)
  {
    return volt_f32{v.value() * 0.5f + 1.65f};
  }

  static constexpr volt_f32 inverse(volt_f32 v)
  {
    return volt_f32{(v.value() - 1.65f) / 0.5f};
  }
};

// a 12-bit converter over a 3.3 V reference
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

// the one stage that carries run-time state
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

// a bound object costs nothing and needs no construction
static_assert(std::is_empty_v<bind<transducer>>);
static_assert(std::is_default_constructible_v<path<bind<transducer>, adc>>);

constexpr bool test_round_trip()
{
  constexpr path<bind<transducer>, conditioning, adc> channel;

  // no current sits at mid-rail
  assert(channel.forward(amp_f32{0.f}) == adc::forward(volt_f32{1.65f}));

  // and what the path adds, the inverse takes back out
  assert(emb::approx(channel.inverse(channel.forward(amp_f32{300.f})),
                     amp_f32{300.f},
                     amp_f32{0.5f}));
  assert(emb::approx(channel.inverse(channel.forward(amp_f32{-450.f})),
                     amp_f32{-450.f},
                     amp_f32{0.5f}));

  // the tolerance above is the quantization referred to the input: one code
  // is 3.3 V / 4095 at the pin, which the board and transducer refer back to
  static_assert(3.3f / 4095.f / 0.5f * 600.f / 4.f < 0.5f);

  return true;
}

constexpr bool test_composition_order()
{
  // forward runs front to back, inverse back to front, so two stages that do
  // not commute tell the two orders apart
  struct add_one {
    static constexpr float forward(float x)
    {
      return x + 1.f;
    }

    static constexpr float inverse(float x)
    {
      return x - 1.f;
    }
  };

  struct twice {
    static constexpr float forward(float x)
    {
      return x * 2.f;
    }

    static constexpr float inverse(float x)
    {
      return x / 2.f;
    }
  };

  constexpr path<add_one, twice> add_then_double;
  constexpr path<twice, add_one> double_then_add;

  assert(emb::approx(add_then_double.forward(3.f), 8.f, 1e-6f));
  assert(emb::approx(double_then_add.forward(3.f), 7.f, 1e-6f));

  assert(emb::approx(add_then_double.inverse(8.f), 3.f, 1e-6f));
  assert(emb::approx(double_then_add.inverse(7.f), 3.f, 1e-6f));

  return true;
}

constexpr bool test_stateful_stage()
{
  // a stage with run-time state is handed in as a value; the rest of the
  // path is still default-constructed alongside it
  path<zero_trim, bind<transducer>> const trimmed{zero_trim{amp_f32{2.5f}},
                                                  {}};

  assert(emb::approx(trimmed.forward(amp_f32{0.f}),
                     transducer.forward(amp_f32{2.5f}),
                     volt_f32{1e-6f}));
  assert(emb::approx(trimmed.inverse(transducer.forward(amp_f32{2.5f})),
                     amp_f32{0.f},
                     amp_f32{1e-4f}));

  // and the trim is the only difference from an untrimmed path
  constexpr path<zero_trim, bind<transducer>> untrimmed;
  assert(emb::approx(untrimmed.forward(amp_f32{2.5f}),
                     trimmed.forward(amp_f32{0.f}),
                     volt_f32{1e-6f}));

  return true;
}

constexpr bool test_single_stage()
{
  constexpr path<bind<transducer>> bare;

  assert(emb::approx(bare.forward(amp_f32{600.f}), volt_f32{4.f}, volt_f32{1e-4f}));
  assert(emb::approx(bare.inverse(volt_f32{4.f}), amp_f32{600.f}, amp_f32{1e-2f}));

  return true;
}

static_assert(test_round_trip());
static_assert(test_composition_order());
static_assert(test_stateful_stage());
static_assert(test_single_stage());

} // namespace
