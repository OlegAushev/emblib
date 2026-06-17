#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

namespace emb::actuator {

// A closed-loop actuator: an open-loop actuator paired with a feedback channel
// that reports its actual state. Provides only the comparison primitives
// (desired vs. actual); any time-dependent policy — debounce, actuation
// timeout, fault latching — belongs to the caller, since the actuator itself
// has no notion of time and the actual state lags a command.
template<typename Actuator, typename Feedback>
class monitored {
public:
  using state_type = typename Actuator::state_type;
  static_assert(
      std::same_as<typename Feedback::state_type, state_type>,
      "feedback and actuator must report the same state type"
  );
  static_assert(
      std::equality_comparable<state_type>,
      "actuator state must be equality-comparable to compare desired vs actual"
  );
private:
  Actuator actuator_;
  Feedback feedback_;
public:
  monitored(Actuator actuator, Feedback feedback)
      : actuator_(std::move(actuator)), feedback_(std::move(feedback)) {}

  void command(state_type s) {
    actuator_.command(s);
  }

  state_type desired() const {
    return actuator_.state();
  }

  state_type actual() {
    return feedback_.read();
  }

  // Whether the actual state currently matches the desired one. Only meaningful
  // once the actuator has had time to settle after a command.
  bool in_sync() {
    return actual() == desired();
  }
};

} // namespace emb::actuator
