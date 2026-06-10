#pragma once

#include <emb/actuator/generic.hpp>

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

} // namespace emb::actuator::discrete
