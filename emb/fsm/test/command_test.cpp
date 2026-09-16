#include <emb/fsm/command.hpp>
#include <emb/fsm/fsm_v3.hpp>

#include <cassert>
#include <optional>
#include <type_traits>
#include <variant>

// deliberately outside emb::fsm::command: a delivery has to compile on the
// strength of the context, its states and its controls alone
namespace {

using emb::fsm::command::channel_for;
using emb::fsm::command::control_of;
using emb::fsm::command::deliver;
using emb::fsm::command::deliver_all;
using emb::fsm::command::deliverable;
using emb::fsm::command::sink_of;
using emb::fsm::command::some_channel;

enum class gear { neutral, forward };

struct speed {
  float kmh = 0.f;
  constexpr bool operator==(speed const&) const = default;
};

class vehicle;

// ---- channels --------------------------------------------------------------

struct gear_channel {
  using command = gear;
  template<typename S>
  using control = typename S::gear_control;
};

struct speed_channel {
  using command = speed;
  template<typename S>
  using control = typename S::speed_control;
};

// the command type of speed_channel, and a channel of its own
struct limit_channel {
  using command = speed;
  template<typename S>
  using control = typename S::limit_control;
};

// only some states name a cruise control
struct cruise_channel {
  using command = speed;
  template<typename S>
  using control = typename S::cruise_control;
};

using channels = emb::typelist<gear_channel, speed_channel, limit_channel>;

// ---- controls --------------------------------------------------------------

// nobody drives: the safe value of every channel
struct idle_control {
  static constexpr gear value(gear_channel, vehicle const&)
  {
    return gear::neutral;
  }

  static constexpr speed value(speed_channel, vehicle const&)
  {
    return speed{};
  }
};

// reads the pedals out of the context and keeps nothing itself
struct driver_control {
  static constexpr gear value(gear_channel, vehicle const& v);
  static constexpr speed value(speed_channel, vehicle const& v);
  static constexpr speed value(cruise_channel, vehicle const& v);
};

// serves the limit channel only, though the speed channel carries the same
// command type
struct limiter_control {
  static constexpr speed value(limit_channel, vehicle const& v);
};

// ---- states ----------------------------------------------------------------

struct parked;
struct rolling;

using next = std::optional<std::variant<parked, rolling>>;

struct go {};
struct halt {};

struct parked {
  static constexpr int id = 0;
  using gear_control = driver_control;
  using speed_control = idle_control;
  using limit_control = limiter_control;
};

struct rolling {
  static constexpr int id = 1;
  using gear_control = driver_control;
  using speed_control = driver_control;
  using limit_control = limiter_control;
  using cruise_control = driver_control;
};

// ---- the context -----------------------------------------------------------

constexpr next on_event(vehicle&, go const&)
{
  return rolling{};
}

constexpr next on_event(vehicle&, halt const&)
{
  return parked{};
}

class vehicle : public emb::fsm::v3::finite_state_machine<
                    vehicle,
                    emb::fsm::v3::mealy_policy,
                    emb::typelist<parked, rolling>> {
public:
  gear pedal_gear = gear::neutral;
  speed pedal_speed{};
  speed speed_limit{};

  gear last_gear = gear::neutral;
  speed last_speed{};
  speed last_limit{};
  speed last_cruise{};
  int deliveries = 0;

  constexpr vehicle() : fsm_type(parked{}) {}

  // a held level turned into an event, the way a drive turns a held start into
  // a transition: accepting this command moves the state machine
  constexpr void accept(gear_channel, gear const& g)
  {
    last_gear = g;
    ++deliveries;
    if (g == gear::forward) {
      dispatch(go{});
    }
  }

  constexpr void accept(speed_channel, speed const& s)
  {
    last_speed = s;
    ++deliveries;
  }

  constexpr void accept(limit_channel, speed const& s)
  {
    last_limit = s;
    ++deliveries;
  }

  constexpr void accept(cruise_channel, speed const& s)
  {
    last_cruise = s;
    ++deliveries;
  }
};

constexpr gear driver_control::value(gear_channel, vehicle const& v)
{
  return v.pedal_gear;
}

constexpr speed driver_control::value(speed_channel, vehicle const& v)
{
  return v.pedal_speed;
}

constexpr speed driver_control::value(cruise_channel, vehicle const& v)
{
  return v.pedal_speed;
}

constexpr speed limiter_control::value(limit_channel, vehicle const& v)
{
  return v.speed_limit;
}

// ---- what a channel is -----------------------------------------------------

static_assert(some_channel<gear_channel>);

// a channel names the command it carries
struct commandless_channel {
  template<typename S>
  using control = typename S::gear_control;
};

static_assert(!some_channel<commandless_channel>);

// and it is passed by value as a key, so it carries nothing
struct laden_channel {
  using command = gear;
  template<typename S>
  using control = typename S::gear_control;

  int payload = 0;
};

static_assert(!some_channel<laden_channel>);

// ---- what a control is -----------------------------------------------------

static_assert(control_of<idle_control, gear_channel, vehicle>);
static_assert(control_of<driver_control, speed_channel, vehicle>);

// a control serves a channel by naming it: the limiter returns the command
// type the speed channel carries, and still is no control for it
static_assert(control_of<limiter_control, limit_channel, vehicle>);
static_assert(!control_of<limiter_control, speed_channel, vehicle>);

// a control that keeps something is not one
struct keeping_control {
  gear kept = gear::neutral;

  static constexpr gear value(gear_channel, vehicle const&)
  {
    return gear::neutral;
  }
};

static_assert(!control_of<keeping_control, gear_channel, vehicle>);

// convertible is not enough
struct odometer_channel {
  using command = long;
  template<typename S>
  using control = typename S::odometer_control;
};

struct widening_control {
  static constexpr int value(odometer_channel, vehicle const&)
  {
    return 0;
  }
};

static_assert(!control_of<widening_control, odometer_channel, vehicle>);

// the documented limit: is_empty_v does not see static members, so a control
// with class-level state still passes. Keeping controls free of it is a review
// rule, not something the concept checks.
struct class_state_control {
  [[maybe_unused]] static inline gear kept = gear::neutral;

  static constexpr gear value(gear_channel, vehicle const&)
  {
    return gear::neutral;
  }
};

static_assert(control_of<class_state_control, gear_channel, vehicle>);

// ---- what a sink is --------------------------------------------------------

struct keyless_sink {
  constexpr void accept(speed const&) {}
};

// one command type, two channels: a sink accepts a channel by naming it
struct speed_only_sink {
  constexpr void accept(speed_channel, speed const&) {}
};

static_assert(sink_of<vehicle, speed_channel>);
static_assert(sink_of<vehicle, limit_channel>);
static_assert(!sink_of<vehicle, odometer_channel>);
static_assert(!sink_of<keyless_sink, speed_channel>);
static_assert(sink_of<speed_only_sink, speed_channel>);
static_assert(!sink_of<speed_only_sink, limit_channel>);

// ---- what is deliverable ---------------------------------------------------

static_assert(channel_for<speed_channel, parked, vehicle>);
static_assert(channel_for<cruise_channel, rolling, vehicle>);
static_assert(!channel_for<cruise_channel, parked, vehicle>);

static_assert(deliverable<gear_channel, vehicle>);
static_assert(deliverable<speed_channel, vehicle>);
static_assert(deliverable<limit_channel, vehicle>);

// exhaustiveness: the vehicle accepts the cruise channel and rolling names a
// control for it, but parked does not
static_assert(!deliverable<cruise_channel, vehicle>);

// A state that names no control, a control that keeps something, a sink that
// does not accept the channel, or a channel listed twice stop delivery with a
// sentence. Those are static_asserts -- a hard error, not a substitution
// failure -- so they cannot be written as negative checks; deliverable<> above
// is the testable half, and the wording is checked by uncommenting:
//
//   constexpr void no_cruise(vehicle& v)
//   {
//     deliver<cruise_channel>(v);
//   }

// ---- delivery --------------------------------------------------------------

constexpr bool test_state_names_the_control()
{
  vehicle v;

  deliver_all<channels>(v);
  assert(v.is_in_state<parked>());
  assert(v.last_gear == gear::neutral);
  assert(v.last_speed == speed{});

  // the pedal is there to read, but in parked nobody drives the speed
  v.pedal_speed = speed{10.f};
  deliver<speed_channel>(v);
  assert(v.last_speed == speed{});

  return true;
}

constexpr bool test_delivery_order()
{
  vehicle v;
  v.pedal_gear = gear::forward;
  v.pedal_speed = speed{10.f};

  // one pass: the gear channel goes first, accepting forward moves the vehicle
  // to rolling, and the speed channel reads the state it moved to
  deliver_all<channels>(v);
  assert(v.is_in_state<rolling>());
  assert(v.last_gear == gear::forward);
  assert(v.last_speed == speed{10.f});
  assert(v.deliveries == 3);

  return true;
}

constexpr bool test_level_semantics()
{
  vehicle v;
  v.pedal_gear = gear::forward;

  // delivered on every pass, not on change
  deliver_all<channels>(v);
  deliver_all<channels>(v);
  deliver_all<channels>(v);
  assert(v.deliveries == 9);
  assert(v.last_gear == gear::forward);

  return true;
}

constexpr bool test_nothing_to_restore()
{
  vehicle v;
  v.pedal_gear = gear::forward;
  v.pedal_speed = speed{10.f};
  deliver_all<channels>(v);
  assert(v.last_speed == speed{10.f});

  // the owner is what the state declares: leaving a state leaves nothing
  // behind that a later state would have to restore
  v.pedal_gear = gear::neutral;
  v.dispatch(halt{});
  deliver_all<channels>(v);
  assert(v.is_in_state<parked>());
  assert(v.last_gear == gear::neutral);
  assert(v.last_speed == speed{});

  return true;
}

constexpr bool test_one_command_type_two_channels()
{
  vehicle v;
  v.pedal_gear = gear::forward;
  v.pedal_speed = speed{10.f};
  v.speed_limit = speed{30.f};

  // speed and limit carry the same command type, and each channel still
  // reaches its own control and its own accept()
  deliver_all<channels>(v);
  assert(v.last_speed == speed{10.f});
  assert(v.last_limit == speed{30.f});

  return true;
}

static_assert(test_state_names_the_control());
static_assert(test_delivery_order());
static_assert(test_level_semantics());
static_assert(test_nothing_to_restore());
static_assert(test_one_command_type_two_channels());

} // namespace
