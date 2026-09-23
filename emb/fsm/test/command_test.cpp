#include <emb/fsm/command.hpp>
#include <emb/fsm/fsm_v3.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <variant>

// deliberately outside emb::fsm::command: a delivery or a read has to compile
// on the strength of the context, its states and its controls alone
namespace {

using emb::fsm::command::channel_for;
using emb::fsm::command::control_of;
using emb::fsm::command::deliver;
using emb::fsm::command::deliver_all;
using emb::fsm::command::deliverable;
using emb::fsm::command::dispatcher_of;
using emb::fsm::command::read;
using emb::fsm::command::readable;
using emb::fsm::command::some_channel;

// the pedal, which the controls read
enum class gear { neutral, forward };

// ---- events ----------------------------------------------------------------

struct go {};
struct halt {};

struct speed {
  float kmh = 0.f;
  constexpr bool operator==(speed const&) const = default;
};

// ---- data ------------------------------------------------------------------

// applied, not reacted to: no handler takes it
struct throttle {
  float percent = 0.f;
  constexpr bool operator==(throttle const&) const = default;
};

class vehicle;

// ---- channels --------------------------------------------------------------

// halt first: a default-constructed command holds the safe event
using gear_command = std::variant<halt, go>;

struct gear_channel {
  using command = gear_command;
  template<typename S>
  using control = typename S::gear_control;
};

struct speed_channel {
  using command = speed;
  template<typename S>
  using control = typename S::speed_control;
};

// the event speed_channel carries, and a channel of its own
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

// data, read and never delivered
struct throttle_channel {
  using command = throttle;
  template<typename S>
  using control = typename S::throttle_control;
};

using channels = emb::typelist<gear_channel, speed_channel, limit_channel>;

// ---- controls --------------------------------------------------------------

// nobody drives: the safe command of every channel
struct idle_control {
  static constexpr gear_command value(gear_channel, vehicle const&)
  {
    return halt{};
  }

  static constexpr speed value(speed_channel, vehicle const&)
  {
    return speed{};
  }

  static constexpr throttle value(throttle_channel, vehicle const&)
  {
    return throttle{};
  }
};

// reads the pedals out of the context and keeps nothing itself
struct driver_control {
  static constexpr gear_command value(gear_channel, vehicle const& v);
  static constexpr speed value(speed_channel, vehicle const& v);
  static constexpr speed value(cruise_channel, vehicle const& v);
  static constexpr throttle value(throttle_channel, vehicle const& v);
};

// serves the limit channel only, though the speed channel carries the same
// event
struct limiter_control {
  static constexpr speed value(limit_channel, vehicle const& v);
};

// ---- states ----------------------------------------------------------------

struct parked;
struct rolling;

using next = std::optional<std::variant<parked, rolling>>;

struct parked {
  static constexpr int id = 0;
  using gear_control = driver_control;
  using speed_control = idle_control;
  using limit_control = limiter_control;
  using throttle_control = idle_control;
};

struct rolling {
  static constexpr int id = 1;
  using gear_control = driver_control;
  using speed_control = driver_control;
  using limit_control = limiter_control;
  using cruise_control = driver_control;
  using throttle_control = driver_control;
};

// ---- the context -----------------------------------------------------------

class vehicle : public emb::fsm::v3::finite_state_machine<
                    vehicle,
                    emb::fsm::v3::mealy_policy,
                    emb::typelist<parked, rolling>> {
public:
  gear pedal_gear = gear::neutral;
  speed pedal_speed{};
  speed speed_limit{};
  throttle pedal_throttle{};

  // what the machine was handed
  gear last_gear = gear::neutral;
  std::array<speed, 8> speeds{}; // in the order they came
  std::size_t speed_count = 0;
  int deliveries = 0;

  constexpr vehicle() : fsm_type(parked{}) {}
};

// The states name controls and handle nothing themselves: every event reaches
// the machine through a common handler.

constexpr next on_event(vehicle& v, go const&)
{
  v.last_gear = gear::forward;
  ++v.deliveries;
  return rolling{};
}

constexpr next on_event(vehicle& v, halt const&)
{
  v.last_gear = gear::neutral;
  ++v.deliveries;
  return parked{};
}

// the speed and limit channels both carry this event, and only the order tells
// them apart
constexpr next on_event(vehicle& v, speed const& s)
{
  v.speeds[v.speed_count++] = s;
  ++v.deliveries;
  return {};
}

constexpr gear_command driver_control::value(gear_channel, vehicle const& v)
{
  if (v.pedal_gear == gear::forward) {
    return go{};
  }
  return halt{};
}

constexpr speed driver_control::value(speed_channel, vehicle const& v)
{
  return v.pedal_speed;
}

constexpr speed driver_control::value(cruise_channel, vehicle const& v)
{
  return v.pedal_speed;
}

constexpr throttle driver_control::value(throttle_channel, vehicle const& v)
{
  return v.pedal_throttle;
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
  using command = gear_command;
  template<typename S>
  using control = typename S::gear_control;

  int payload = 0;
};

static_assert(!some_channel<laden_channel>);

// ---- what a control is -----------------------------------------------------

static_assert(control_of<idle_control, gear_channel, vehicle>);
static_assert(control_of<driver_control, speed_channel, vehicle>);

// a control serves a channel by naming it: the limiter returns the event the
// speed channel carries, and still is no control for it
static_assert(control_of<limiter_control, limit_channel, vehicle>);
static_assert(!control_of<limiter_control, speed_channel, vehicle>);

// a control that keeps something is not one
struct keeping_control {
  gear kept = gear::neutral;

  static constexpr gear_command value(gear_channel, vehicle const&)
  {
    return halt{};
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

  static constexpr gear_command value(gear_channel, vehicle const&)
  {
    return halt{};
  }
};

static_assert(control_of<class_state_control, gear_channel, vehicle>);

// ---- what the machine dispatches -------------------------------------------

// one event, and a variant of events every one of which has a handler
static_assert(dispatcher_of<vehicle, speed_channel>);
static_assert(dispatcher_of<vehicle, gear_channel>);

// a command that is no event of the machine's
static_assert(!dispatcher_of<vehicle, odometer_channel>);

// a variant is dispatched only if every event it can hold is: nothing handles
// tow
struct tow {};

struct towing_channel {
  using command = std::variant<halt, go, tow>;
  template<typename S>
  using control = typename S::gear_control;
};

static_assert(!dispatcher_of<vehicle, towing_channel>);

// ---- what is readable ------------------------------------------------------

// every state names a control, and nothing more is asked of the machine: no
// handler takes a throttle
static_assert(readable<throttle_channel, vehicle>);

// a channel that carries an event is read the same way
static_assert(readable<speed_channel, vehicle>);

// exhaustiveness: rolling names a cruise control, but parked does not
static_assert(!readable<cruise_channel, vehicle>);

// ---- what is deliverable ---------------------------------------------------

static_assert(channel_for<speed_channel, parked, vehicle>);
static_assert(channel_for<cruise_channel, rolling, vehicle>);
static_assert(!channel_for<cruise_channel, parked, vehicle>);

static_assert(deliverable<gear_channel, vehicle>);
static_assert(deliverable<speed_channel, vehicle>);
static_assert(deliverable<limit_channel, vehicle>);

// exhaustiveness: the vehicle dispatches the cruise channel's event and rolling
// names a control for it, but parked does not
static_assert(!deliverable<cruise_channel, vehicle>);

// data is read, never delivered: every state names a throttle control, and
// still no handler takes a throttle
static_assert(!deliverable<throttle_channel, vehicle>);

// ---- what compiles ---------------------------------------------------------

// read(), deliver() and deliver_all() are constrained on the concepts above, so
// a call they reject is no match: it does not compile, and that is checked here
template<typename Channel>
concept reads = requires(vehicle const& v) { read<Channel>(v); };

template<typename Channel>
concept delivers = requires(vehicle& v) { deliver<Channel>(v); };

template<typename ChannelList>
concept delivers_all = requires(vehicle& v) { deliver_all<ChannelList>(v); };

static_assert(reads<throttle_channel>);
static_assert(delivers<gear_channel>);
static_assert(delivers_all<channels>);

// parked names no cruise control
static_assert(!reads<cruise_channel>);
static_assert(!delivers<cruise_channel>);

// data: no handler takes a throttle
static_assert(!delivers<throttle_channel>);

// a channel listed twice, a list of none, and no list at all
static_assert(!delivers_all<emb::typelist<gear_channel, gear_channel>>);
static_assert(!delivers_all<emb::typelist<>>);
static_assert(!delivers_all<gear_channel>);

// ---- delivery --------------------------------------------------------------

constexpr bool test_state_names_the_control()
{
  vehicle v;

  deliver_all<channels>(v);
  assert(v.is_in_state<parked>());
  assert(v.last_gear == gear::neutral);
  assert(v.speeds[0] == speed{});

  // the pedal is there to read, but in parked nobody drives the speed
  v.pedal_speed = speed{10.f};
  deliver<speed_channel>(v);
  assert(v.speeds[2] == speed{});

  return true;
}

constexpr bool test_delivery_order()
{
  vehicle v;
  v.pedal_gear = gear::forward;
  v.pedal_speed = speed{10.f};

  // one pass: the gear channel goes first, dispatching go moves the vehicle to
  // rolling, and the speed channel reads the state it moved to; the variant
  // hands over the one event it holds
  deliver_all<channels>(v);
  assert(v.is_in_state<rolling>());
  assert(v.last_gear == gear::forward);
  assert(v.speeds[0] == speed{10.f});
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
  assert(v.speeds[0] == speed{10.f});

  // the owner is what the state declares: leaving a state leaves nothing
  // behind that a later state would have to restore
  v.pedal_gear = gear::neutral;
  v.dispatch(halt{});
  deliver_all<channels>(v);
  assert(v.is_in_state<parked>());
  assert(v.last_gear == gear::neutral);
  assert(v.speeds[2] == speed{});

  return true;
}

constexpr bool test_one_event_two_channels()
{
  vehicle v;
  v.pedal_gear = gear::forward;
  v.pedal_speed = speed{10.f};
  v.speed_limit = speed{30.f};

  // speed and limit carry the same event: each channel still reaches its own
  // control, and the machine takes both through one handler, in list order
  deliver_all<channels>(v);
  assert(v.speed_count == 2);
  assert(v.speeds[0] == speed{10.f});
  assert(v.speeds[1] == speed{30.f});

  return true;
}

// ---- reading ---------------------------------------------------------------

constexpr bool test_read_follows_the_state()
{
  vehicle v;
  v.pedal_throttle = throttle{40.f};

  // the pedal is there to read, but in parked nobody drives the throttle
  assert(read<throttle_channel>(v) == throttle{});

  // go moves the vehicle to rolling, and the next read asks the control that
  // rolling names
  v.pedal_gear = gear::forward;
  deliver<gear_channel>(v);
  assert(v.is_in_state<rolling>());
  assert(read<throttle_channel>(v) == throttle{40.f});

  return true;
}

constexpr bool test_read_dispatches_nothing()
{
  vehicle v;
  v.pedal_gear = gear::forward;

  // reading go leaves the vehicle parked, and reading a speed hands the
  // machine nothing
  assert(std::holds_alternative<go>(read<gear_channel>(v)));
  assert(v.is_in_state<parked>());
  assert(read<speed_channel>(v) == speed{});
  assert(v.deliveries == 0);

  return true;
}

static_assert(test_state_names_the_control());
static_assert(test_delivery_order());
static_assert(test_level_semantics());
static_assert(test_nothing_to_restore());
static_assert(test_one_event_two_channels());
static_assert(test_read_follows_the_state());
static_assert(test_read_dispatches_nothing());

} // namespace
