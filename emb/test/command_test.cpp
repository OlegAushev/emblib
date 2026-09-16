#include <emb/command.hpp>
#include <emb/fsm/fsm_v3.hpp>

#include <cassert>
#include <optional>
#include <type_traits>
#include <variant>

// deliberately outside emb::command: a delivery has to compile on the strength
// of the context, its states and its controls alone
namespace {

using emb::command::tag;

enum class gear { neutral, forward };

struct speed {
  float kmh = 0.f;
  constexpr bool operator==(speed const&) const = default;
};

class vehicle;

// ---- controls --------------------------------------------------------------

// nobody drives: the safe value of every command
struct idle_control {
  static constexpr gear value(tag<gear>, vehicle const&)
  {
    return gear::neutral;
  }

  static constexpr speed value(tag<speed>, vehicle const&)
  {
    return speed{};
  }
};

// reads the pedals out of the context and keeps nothing itself
struct driver_control {
  static constexpr gear value(tag<gear>, vehicle const& v);
  static constexpr speed value(tag<speed>, vehicle const& v);
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
};

struct rolling {
  static constexpr int id = 1;
  using gear_control = driver_control;
  using speed_control = driver_control;
  using cruise_control = driver_control;
};

// ---- channels --------------------------------------------------------------

struct gear_channel {
  using command_type = gear;
  template<typename S>
  using control = typename S::gear_control;
};

struct speed_channel {
  using command_type = speed;
  template<typename S>
  using control = typename S::speed_control;
};

// only some states name a cruise control
struct cruise_channel {
  using command_type = speed;
  template<typename S>
  using control = typename S::cruise_control;
};

using channels = emb::typelist<gear_channel, speed_channel>;

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

  gear last_gear = gear::neutral;
  speed last_speed{};
  int deliveries = 0;

  constexpr vehicle() : fsm_type(parked{}) {}

  // a held level turned into an event, the way a drive turns a held start into
  // a transition: accepting this command moves the state machine
  constexpr void accept(tag<gear>, gear const& g)
  {
    last_gear = g;
    ++deliveries;
    if (g == gear::forward) {
      dispatch(go{});
    }
  }

  constexpr void accept(tag<speed>, speed const& s)
  {
    last_speed = s;
    ++deliveries;
  }
};

constexpr gear driver_control::value(tag<gear>, vehicle const& v)
{
  return v.pedal_gear;
}

constexpr speed driver_control::value(tag<speed>, vehicle const& v)
{
  return v.pedal_speed;
}

// ---- what a control is -----------------------------------------------------

static_assert(emb::command::control_of<idle_control, gear, vehicle>);
static_assert(emb::command::control_of<driver_control, speed, vehicle>);

// a control that keeps something is not one
struct keeping_control {
  gear kept = gear::neutral;

  static constexpr gear value(tag<gear>, vehicle const&)
  {
    return gear::neutral;
  }
};

static_assert(!emb::command::control_of<keeping_control, gear, vehicle>);

// convertible is not enough
struct widening_control {
  static constexpr int value(tag<long>, vehicle const&)
  {
    return 0;
  }
};

static_assert(!emb::command::control_of<widening_control, long, vehicle>);

// the documented limit: is_empty_v does not see static members, so a control
// with class-level state still passes. Keeping controls free of it is a review
// rule, not something the concept checks.
struct class_state_control {
  [[maybe_unused]] static inline gear kept = gear::neutral;

  static constexpr gear value(tag<gear>, vehicle const&)
  {
    return gear::neutral;
  }
};

static_assert(emb::command::control_of<class_state_control, gear, vehicle>);

// ---- what a sink is --------------------------------------------------------

struct untagged_sink {
  constexpr void accept(speed const&) {}
};

static_assert(emb::command::sink_of<vehicle, speed>);
static_assert(!emb::command::sink_of<untagged_sink, speed>);
static_assert(!emb::command::sink_of<vehicle, long>);

// ---- what is deliverable ---------------------------------------------------

static_assert(emb::command::channel_for<speed_channel, parked, vehicle>);
static_assert(emb::command::channel_for<cruise_channel, rolling, vehicle>);
static_assert(!emb::command::channel_for<cruise_channel, parked, vehicle>);

static_assert(emb::command::deliverable<gear_channel, vehicle>);
static_assert(emb::command::deliverable<speed_channel, vehicle>);

// exhaustiveness: rolling names a cruise control and parked does not
static_assert(!emb::command::deliverable<cruise_channel, vehicle>);

// A state that names no control, a control that keeps something, or a sink that
// does not accept the command stop deliver() with a sentence. Those are
// static_asserts -- a hard error, not a substitution failure -- so they cannot
// be written as negative checks; deliverable<> above is the testable half, and
// the wording is checked by uncommenting:
//
//   constexpr void no_cruise(vehicle& v)
//   {
//     emb::command::deliver<cruise_channel>(v);
//   }

// ---- delivery --------------------------------------------------------------

constexpr bool test_state_names_the_control()
{
  vehicle v;

  emb::command::deliver_all<channels>(v);
  assert(v.is_in_state<parked>());
  assert(v.last_gear == gear::neutral);
  assert(v.last_speed == speed{});

  // the pedal is there to read, but in parked nobody drives the speed
  v.pedal_speed = speed{10.f};
  emb::command::deliver<speed_channel>(v);
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
  emb::command::deliver_all<channels>(v);
  assert(v.is_in_state<rolling>());
  assert(v.last_gear == gear::forward);
  assert(v.last_speed == speed{10.f});
  assert(v.deliveries == 2);

  return true;
}

constexpr bool test_level_semantics()
{
  vehicle v;
  v.pedal_gear = gear::forward;

  // delivered on every pass, not on change
  emb::command::deliver_all<channels>(v);
  emb::command::deliver_all<channels>(v);
  emb::command::deliver_all<channels>(v);
  assert(v.deliveries == 6);
  assert(v.last_gear == gear::forward);

  return true;
}

constexpr bool test_nothing_to_restore()
{
  vehicle v;
  v.pedal_gear = gear::forward;
  v.pedal_speed = speed{10.f};
  emb::command::deliver_all<channels>(v);
  assert(v.last_speed == speed{10.f});

  // the owner is what the state declares: leaving a state leaves nothing
  // behind that a later state would have to restore
  v.pedal_gear = gear::neutral;
  v.dispatch(halt{});
  emb::command::deliver_all<channels>(v);
  assert(v.is_in_state<parked>());
  assert(v.last_gear == gear::neutral);
  assert(v.last_speed == speed{});

  return true;
}

static_assert(test_state_names_the_control());
static_assert(test_delivery_order());
static_assert(test_level_semantics());
static_assert(test_nothing_to_restore());

} // namespace
