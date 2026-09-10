#include <emb/signal/sign.hpp>

#include <emb/math.hpp>
#include <emb/signal/bind.hpp>
#include <emb/signal/path.hpp>
#include <emb/signal/proportional.hpp>
#include <emb/units.hpp>

#include <cstdint>
#include <type_traits>

namespace {

using namespace emb::units;
using emb::signal::identity;
using emb::signal::negation;
using emb::signal::path;

// neither carries anything, so a path built from one costs what the rest of
// the path costs
static_assert(std::is_empty_v<identity>);
static_assert(std::is_empty_v<negation>);

// a sign convention is about a quantity that has a sign; a raw code has none,
// and negating one either wraps around or lands in another type
static_assert(emb::signal::sign_reversible<amp_f32>);
static_assert(emb::signal::sign_reversible<float>);
static_assert(emb::signal::sign_reversible<std::int32_t>);
static_assert(!emb::signal::sign_reversible<std::uint16_t>);
static_assert(!emb::signal::sign_reversible<std::uint32_t>);

// so negation refuses a code, at the point of use rather than inside itself,
// while identity carries one without complaint
template<typename T>
constexpr bool negates =
    requires(T x) { negation::forward(x); negation::inverse(x); };
template<typename T>
constexpr bool passes =
    requires(T x) { identity::forward(x); identity::inverse(x); };

static_assert(negates<amp_f32>);
static_assert(negates<std::int32_t>);
static_assert(!negates<std::uint16_t>);
static_assert(!negates<std::uint32_t>);
static_assert(passes<std::uint16_t>);

// a transducer reading a current as a voltage, to sit behind the convention
inline constexpr emb::signal::proportional transducer{amp_f32{600.f},
                                                      volt_f32{4.f}};

// the choice a channel is parameterized over
template<typename Convention>
using head = path<Convention, emb::signal::bind<transducer>>;

constexpr bool test_the_pair_differs_by_sign()
{
  [[maybe_unused]] constexpr head<identity> along;
  [[maybe_unused]] constexpr head<negation> against;

  // the two paths disagree about which way the current runs, and about
  // nothing else
  assert(emb::approx(along.forward(amp_f32{150.f}),
                     -against.forward(amp_f32{150.f}),
                     volt_f32{1e-6f}));
  assert(emb::approx(along.inverse(volt_f32{1.f}),
                     -against.inverse(volt_f32{1.f}),
                     amp_f32{1e-3f}));

  // zero is the one reading they agree on
  assert(emb::approx(along.forward(amp_f32{0.f}),
                     against.forward(amp_f32{0.f}),
                     volt_f32{1e-6f}));

  return true;
}

constexpr bool test_each_is_its_own_inverse()
{
  assert(emb::approx(negation::inverse(negation::forward(amp_f32{37.5f})),
                     amp_f32{37.5f},
                     amp_f32{1e-6f}));
  assert(emb::approx(identity::inverse(identity::forward(amp_f32{37.5f})),
                     amp_f32{37.5f},
                     amp_f32{1e-6f}));

  // and negation, unlike identity, is not the thing it is applied to
  assert(emb::approx(negation::forward(amp_f32{37.5f}),
                     amp_f32{-37.5f},
                     amp_f32{1e-6f}));

  return true;
}

constexpr bool test_carries_any_quantity()
{
  // the stages fix a sign, not a unit: whatever runs through keeps its own
  assert(emb::approx(negation::forward(volt_f32{2.f}),
                     volt_f32{-2.f},
                     volt_f32{1e-6f}));
  assert(emb::approx(negation::forward(degree_celsius_f32{20.f}),
                     degree_celsius_f32{-20.f},
                     degree_celsius_f32{1e-6f}));

  return true;
}

static_assert(test_the_pair_differs_by_sign());
static_assert(test_each_is_its_own_inverse());
static_assert(test_carries_any_quantity());

} // namespace
