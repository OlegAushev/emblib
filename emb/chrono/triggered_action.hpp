#pragma once

#include <chrono>

namespace emb::chrono {

template<typename Clock>
  requires std::chrono::is_clock_v<Clock>
class triggered_action {
public:
  using duration = Clock::duration;
  using time_point = Clock::time_point;
private:
  bool (*trigger_)();
  duration delay_;
  void (*action_)();

  bool trigger_detected_;
  time_point trigger_timepoint_;
public:
  triggered_action(bool (*trigger)(), duration delay, void (*action)())
      : trigger_(trigger),
        delay_(delay),
        action_(action),
        trigger_detected_(false),
        trigger_timepoint_(Clock::now())
  {
    check_and_execute();
  }

  void check_and_execute()
  {
    if (trigger_()) {
      auto now = Clock::now();
      if (!trigger_detected_) {
        trigger_detected_ = true;
        trigger_timepoint_ = now;
      }
      if (now > trigger_timepoint_ + delay_) {
        action_();
      }
    }
    else {
      trigger_detected_ = false;
    }
  }
};

} // namespace emb::chrono
