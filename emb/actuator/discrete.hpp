#pragma once

#include <emb/actuator/feedback.hpp>
#include <emb/actuator/monitored.hpp>
#include <emb/actuator/unmonitored.hpp>

#include <emb/gpio.hpp>

#include <cstdint>

namespace emb::actuator::discrete {

// Two-position state of a discrete actuator.
enum class position : std::uint8_t { closed, open };

// Logical drive/feedback signal of a discrete actuator, independent of the
// medium that carries it: encoders emit it, decoders consume it. A concrete
// driver/sensor (e.g. the GPIO ones below) translates it to and from hardware.
enum class signal : std::uint8_t { inactive, active };

// Position<->signal bijection for a normally-closed actuator: de-energized rest
// is closed. As an encoder it maps a desired position to a drive signal; as a
// decoder it maps a feedback signal back to a position (active = open, i.e. a
// break aux contact).
struct normally_closed {
  static constexpr signal operator()(position desired) {
    return desired != position::closed ? signal::active : signal::inactive;
  }
  static constexpr position operator()(signal s) {
    return s == signal::active ? position::open : position::closed;
  }
};

// Position<->signal bijection for a normally-open actuator: de-energized rest
// is open. As an encoder it maps a desired position to a drive signal; as a
// decoder it maps a feedback signal back to a position (active = closed, i.e. a
// make aux contact).
struct normally_open {
  static constexpr signal operator()(position desired) {
    return desired != position::open ? signal::active : signal::inactive;
  }
  static constexpr position operator()(signal s) {
    return s == signal::active ? position::closed : position::open;
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

  void operator()(signal s) {
    pin_.set(
        s == signal::active ? emb::gpio::state::active
                            : emb::gpio::state::inactive
    );
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

  signal operator()() const {
    return pin_.read() == emb::gpio::state::active ? signal::active
                                                   : signal::inactive;
  }
};

// A two-position actuator driven by a single GPIO line.
template<typename Encoder, typename Pin>
using device = unmonitored<position, Encoder, gpio_driver<Pin>>;

// A feedback channel sensing position through a GPIO input pin.
template<typename Decoder, typename Pin>
using sense = feedback<position, gpio_sensor<Pin>, Decoder>;

// A two-position actuator with closed-loop position feedback: a control output
// pin plus a feedback input pin.
template<typename Encoder, typename Decoder, typename CtrlPin, typename FbPin>
using monitored_device =
    monitored<device<Encoder, CtrlPin>, sense<Decoder, FbPin>>;

} // namespace emb::actuator::discrete
