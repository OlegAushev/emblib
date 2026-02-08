#pragma once

#include <chrono>

namespace emb {
namespace chrono {

class steady_clock {
public:
  using duration = std::chrono::milliseconds;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<steady_clock, duration>;
  static constexpr bool is_steady = true;
public:
  static std::chrono::time_point<steady_clock> now();
};

static_assert(std::chrono::is_clock_v<steady_clock>);

class watchdog {
public:
  using duration = steady_clock::duration;
  using time_point = steady_clock::time_point;
private:
  duration timeout_;
  time_point start_;
public:
  watchdog(duration timeout = duration{0})
      : timeout_{timeout}, start_{emb::chrono::steady_clock::now()} {}

  bool good() const {
    if (timeout_.count() < 0) {
      return true;
    }
    if ((steady_clock::now() - start_) > timeout_) {
      return false;
    }
    return true;
  }

  bool bad() const {
    return !good();
  }

  void reset() {
    start_ = steady_clock::now();
  }

  void reset(duration timeout) {
    timeout_ = timeout;
    start_ = steady_clock::now();
  }
};

class triggered_action {
public:
  using duration = steady_clock::duration;
  using time_point = steady_clock::time_point;
private:
  bool (*trigger_)();
  duration delay_;
  void (*action_)();

  bool trigger_detected_;
  time_point trigger_timepoint_;
public:
  triggered_action(bool (*trigger)(), duration delay, void (*action)())
      : trigger_{trigger},
        delay_{delay},
        action_{action},
        trigger_detected_{false},
        trigger_timepoint_{emb::chrono::steady_clock::now()} {
    check_and_execute();
  }

  void check_and_execute() {
    if (trigger_()) {
      auto now{emb::chrono::steady_clock::now()};
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

using nanoseconds_i32 = std::chrono::duration<int32_t, std::nano>;
using microseconds_i32 = std::chrono::duration<int32_t, std::micro>;
using milliseconds_i32 = std::chrono::duration<int32_t, std::milli>;
using seconds_i32 = std::chrono::duration<int32_t>;

} // namespace chrono
} // namespace emb
