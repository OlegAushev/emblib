#pragma once

#include <emb/actuator/concepts.hpp>

#include <utility>

namespace emb::actuator {

// A commandable actuator with no feedback: holds a desired state, encodes it to
// a command, and applies it through a driver. state() reports the last command,
// not the actual hardware state.
template<typename State, typename Encoder, typename Driver>
  requires some_encoder<Encoder, State>
        && some_driver<Driver, std::invoke_result_t<Encoder&, State>>
class unmonitored {
public:
  using state_type = State;
  using command_type = std::invoke_result_t<Encoder&, State>;
private:
  Encoder encoder_;
  Driver driver_;
  State desired_;
public:
  unmonitored(Encoder encoder, Driver driver, State initial)
      : encoder_(std::move(encoder)),
        driver_(std::move(driver)),
        desired_(initial) {
    driver_(encoder_(desired_));
  }

  void command(State s) {
    desired_ = s;
    driver_(encoder_(desired_));
  }

  State state() const {
    return desired_;
  }
};

} // namespace emb::actuator
