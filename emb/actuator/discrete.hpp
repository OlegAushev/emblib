#pragma once

#include <emb/actuator/feedback.hpp>
#include <emb/actuator/monitored.hpp>
#include <emb/actuator/unmonitored.hpp>

#include <emb/gpio.hpp>

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace emb::actuator::discrete {

// Logical drive/feedback signal of a discrete actuator, independent of the
// medium that carries it: encoders emit it, decoders consume it. A concrete
// driver/sensor (e.g. the GPIO ones below) translates it to and from hardware.
enum class signal : std::uint8_t { inactive, active };

// Binds a two-position state type onto the open/closed axis this module speaks:
// which of its values is the open position and which the closed one. Naming
// those values, and numbering them, stays with whoever declares the state, so
// that a contactor, a valve and a brake each keep their own vocabulary.
template<typename P>
concept some_positions = requires {
  requires std::same_as<std::remove_cv_t<decltype(P::open)>,
                        std::remove_cv_t<decltype(P::closed)>>;
  requires std::equality_comparable<decltype(P::open)>;
  // Via bool_constant: a comparison that is no constant expression (non-static
  // members) then fails substitution instead of making the concept ill-formed.
  requires std::bool_constant<(P::open != P::closed)>::value;
};

// Position<->signal bijection for a normally-closed actuator: de-energized rest
// is closed. As a decoder, active = open, i.e. a break aux contact.
template<some_positions P>
struct normally_closed {
  using position_type = std::remove_cv_t<decltype(P::open)>;

  static constexpr signal operator()(position_type desired)
  {
    return desired == P::open ? signal::active : signal::inactive;
  }
  static constexpr position_type operator()(signal s)
  {
    return s == signal::active ? P::open : P::closed;
  }
};

// Position<->signal bijection for a normally-open actuator: de-energized rest
// is open. As a decoder, active = closed, i.e. a make aux contact.
template<some_positions P>
struct normally_open {
  using position_type = std::remove_cv_t<decltype(P::open)>;

  static constexpr signal operator()(position_type desired)
  {
    return desired == P::closed ? signal::active : signal::inactive;
  }
  static constexpr position_type operator()(signal s)
  {
    return s == signal::active ? P::closed : P::open;
  }
};

// Driver translating a logical signal to a GPIO output pin. The pin applies its
// own polarity when mapping the active/inactive state to a physical level.
template<emb::gpio::output Pin>
class gpio_driver {
private:
  Pin& pin_;
public:
  explicit gpio_driver(Pin& pin) : pin_(pin) {}

  void operator()(signal s)
  {
    pin_.set(s == signal::active ? emb::gpio::state::active
                                 : emb::gpio::state::inactive);
  }
};

// Sensor translating a GPIO input pin to a logical signal. The pin applies its
// own polarity when mapping the physical level to the active/inactive state.
template<emb::gpio::input Pin>
class gpio_sensor {
private:
  Pin& pin_;
public:
  explicit gpio_sensor(Pin& pin) : pin_(pin) {}

  signal operator()() const
  {
    return pin_.read() == emb::gpio::state::active ? signal::active
                                                   : signal::inactive;
  }
};

// A two-position actuator driven by a single GPIO line.
template<typename Encoder, typename Pin>
using gpio_actuator =
    unmonitored<typename Encoder::position_type, Encoder, gpio_driver<Pin>>;

// A feedback channel sensing position through a GPIO input pin.
template<typename Decoder, typename Pin>
using gpio_feedback =
    feedback<typename Decoder::position_type, gpio_sensor<Pin>, Decoder>;

// A two-position actuator with closed-loop position feedback: a control output
// pin plus a feedback input pin.
template<typename Encoder, typename Decoder, typename CtrlPin, typename FbPin>
using gpio_monitored_actuator =
    monitored<gpio_actuator<Encoder, CtrlPin>, gpio_feedback<Decoder, FbPin>>;

} // namespace emb::actuator::discrete
