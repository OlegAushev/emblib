#pragma once

#include <chrono>

namespace emb::chrono {

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

  static timeout infinite()
  {
    return timeout(duration::max());
  }

  bool expired() const
  {
    return (Clock::now() - start_) > duration_;
  }

  duration elapsed() const
  {
    return Clock::now() - start_;
  }

  void reset()
  {
    start_ = Clock::now();
  }

  void reset(duration d)
  {
    duration_ = d;
    start_ = Clock::now();
  }
};

} // namespace emb::chrono
