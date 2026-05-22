#pragma once

#include <chrono>
#include <cstdint>

namespace emb {
namespace chrono {

template<typename Clock>
  requires std::chrono::is_clock_v<Clock>
class timeout {
public:
  using duration = Clock::duration;
  using time_point = Clock::time_point;
private:
  duration duration_;
  time_point start_;
public:
  explicit timeout(duration d) : duration_(d), start_(Clock::now()) {}

  static timeout infinite() {
    return timeout(duration::max());
  }

  bool expired() const {
    return (Clock::now() - start_) > duration_;
  }

  duration elapsed() const {
    return Clock::now() - start_;
  }

  void reset() {
    start_ = Clock::now();
  }

  void reset(duration d) {
    duration_ = d;
    start_ = Clock::now();
  }
};

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
        trigger_timepoint_(Clock::now()) {
    check_and_execute();
  }

  void check_and_execute() {
    if (trigger_()) {
      auto now = Clock::now();
      if (!trigger_detected_) {
        trigger_detected_ = true;
        trigger_timepoint_ = now;
      }
      if (now > trigger_timepoint_ + delay_) {
        action_();
      }
    } else {
      trigger_detected_ = false;
    }
  }
};

using nanoseconds_i32 = std::chrono::duration<std::int32_t, std::nano>;
using microseconds_i32 = std::chrono::duration<std::int32_t, std::micro>;
using milliseconds_i32 = std::chrono::duration<std::int32_t, std::milli>;
using seconds_i32 = std::chrono::duration<std::int32_t>;

} // namespace chrono
} // namespace emb
