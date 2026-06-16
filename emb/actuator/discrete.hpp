#pragma once

#include <emb/actuator/feedback.hpp>
#include <emb/actuator/generic.hpp>
#include <emb/actuator/monitored.hpp>

#include <emb/gpio.hpp>

#include <cstdint>

namespace emb::actuator::discrete {

// Two-position state of a discrete actuator.
enum class position : std::uint8_t { closed, open };

// Encoder for a normally-closed actuator: de-energized rest is closed.
struct normally_closed {
  static constexpr emb::gpio::state operator()(position desired) {
    return desired != position::closed ? emb::gpio::state::active
                                       : emb::gpio::state::inactive;
  }
};

// Encoder for a normally-open actuator: de-energized rest is open.
struct normally_open {
  static constexpr emb::gpio::state operator()(position desired) {
    return desired != position::open ? emb::gpio::state::active
                                     : emb::gpio::state::inactive;
  }
};

// Driver applying the encoded state to a GPIO output pin.
template<emb::gpio::output Pin>
class gpio_driver {
private:
  Pin& pin_;
public:
  explicit gpio_driver(Pin& pin) : pin_(pin) {}

  void operator()(emb::gpio::state s) {
    pin_.set(s);
  }
};

// A two-position actuator driven by a single GPIO line.
template<typename Encoder, typename Pin>
using device = generic<position, Encoder, gpio_driver<Pin>>;

// Sensor reading a feedback line from a GPIO input pin.
template<emb::gpio::input Pin>
class gpio_sensor {
private:
  Pin& pin_;
public:
  explicit gpio_sensor(Pin& pin) : pin_(pin) {}

  emb::gpio::state operator()() const {
    return pin_.read();
  }
};

// Decoder for a make (normally-open) aux contact: an active feedback line means
// the actuator has reached the closed position.
struct make_contact {
  static constexpr position operator()(emb::gpio::state s) {
    return s == emb::gpio::state::active ? position::closed : position::open;
  }
};

// Decoder for a break (normally-closed) aux contact: an active feedback line
// means the actuator has reached the open position.
struct break_contact {
  static constexpr position operator()(emb::gpio::state s) {
    return s == emb::gpio::state::active ? position::open : position::closed;
  }
};

// A feedback channel sensing position through a GPIO input pin.
template<typename Decoder, typename Pin>
using sense = feedback<position, gpio_sensor<Pin>, Decoder>;

// A two-position actuator with closed-loop position feedback: a control output
// pin plus a feedback input pin.
template<typename Encoder, typename Decoder, typename CtrlPin, typename FbPin>
using monitored_device =
    monitored<device<Encoder, CtrlPin>, sense<Decoder, FbPin>>;

} // namespace emb::actuator::discrete
