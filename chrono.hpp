#pragma once

#include <emblib/core.hpp>
#if __cplusplus >= 201100
#include <chrono>
#endif

#if __cplusplus >= 201100
#define EMB_MILLISECONDS std::chrono::milliseconds
#else
#define EMB_MILLISECONDS emb::chrono::milliseconds
#endif

namespace emb {
namespace chrono {

#if __cplusplus < 201100

namespace impl {

template<int64_t Divider>
class duration {
public:
  static int64_t const divider = Divider;
private:
  int64_t _ticks;
public:
  duration() : _ticks(0) {}

  explicit duration(int64_t tick_count) : _ticks(tick_count) {}

  duration(duration const& other) { this->_ticks = other._ticks; }

  duration& operator=(duration const& other) {
    if (this != &other) {
      this->_ticks = other._ticks;
    }
    return *this;
  }

  int64_t count() const { return _ticks; }

  duration& operator++() {
    ++_ticks;
    return *this;
  }

  duration& operator--() {
    --_ticks;
    return *this;
  }

  duration operator++(int) {
    duration tmp(*this);
    ++_ticks;
    return tmp;
  }

  duration operator--(int) {
    duration tmp(*this);
    --_ticks;
    return tmp;
  }

  duration& operator+=(duration const& rhs) {
    _ticks += rhs._ticks;
    return *this;
  }

  duration& operator-=(duration const& rhs) {
    _ticks -= rhs._ticks;
    return *this;
  }
};

template<int64_t Divider>
duration<Divider> operator+(duration<Divider> const& lhs,
                            duration<Divider> const& rhs) {
  duration<Divider> tmp = lhs;
  return tmp += rhs;
}

template<int64_t Divider>
duration<Divider> operator-(duration<Divider> const& lhs,
                            duration<Divider> const& rhs) {
  duration<Divider> tmp = lhs;
  return tmp -= rhs;
}

template<int64_t Divider>
bool operator>(duration<Divider> const& lhs, duration<Divider> const& rhs) {
  return lhs.count() > rhs.count();
}

template<int64_t Divider>
bool operator>=(duration<Divider> const& lhs, duration<Divider> const& rhs) {
  return lhs.count() >= rhs.count();
}

template<int64_t Divider>
bool operator<(duration<Divider> const& lhs, duration<Divider> const& rhs) {
  return lhs.count() < rhs.count();
}

template<int64_t Divider>
bool operator<=(duration<Divider> const& lhs, duration<Divider> const& rhs) {
  return lhs.count() <= rhs.count();
}

template<int64_t Divider>
bool operator==(duration<Divider> const& lhs, duration<Divider> const& rhs) {
  return lhs.count() == rhs.count();
}

template<int64_t Divider>
bool operator!=(duration<Divider> const& lhs, duration<Divider> const& rhs) {
  return lhs.count() != rhs.count();
}

} // namespace impl

template<typename ToDuration, int64_t Divider>
ToDuration duration_cast(impl::duration<Divider> const duration) {
  return impl::duration<ToDuration::divider>(duration.count() * Divider /
                                             ToDuration::divider);
}

typedef impl::duration<1> nanoseconds;
typedef impl::duration<1000> microseconds;
typedef impl::duration<1000000> milliseconds;
typedef impl::duration<1000000000> seconds;

#endif

class steady_clock {
private:
  steady_clock();
  steady_clock(steady_clock const& other);
  steady_clock& operator=(steady_clock const& other);

  static EMB_MILLISECONDS (*_now)();

  static EMB_MILLISECONDS _default_now_getter() { return EMB_MILLISECONDS(0); }

  static bool _initialized;
public:
  static void init(EMB_MILLISECONDS (*now_getter)()) {
    _now = now_getter;
    _initialized = true;
  }

  static EMB_MILLISECONDS now() { return _now(); }

  static bool initialized() { return _initialized; }
};

class watchdog {
private:
  EMB_MILLISECONDS _timeout;
  EMB_MILLISECONDS _start;
public:
  watchdog(EMB_MILLISECONDS timeout = EMB_MILLISECONDS(0))
      : _timeout(timeout), _start(emb::chrono::steady_clock::now()) {}

  bool good() const {
    if (_timeout.count() < 0) {
      return true;
    }
    if ((steady_clock::now() - _start) > _timeout) {
      return false;
    }
    return true;
  }

  bool bad() const { return !good(); }

  void reset() { _start = steady_clock::now(); }

  void reset(EMB_MILLISECONDS timeout) {
    _timeout = timeout;
    _start = steady_clock::now();
  }
};

class triggered_action {
private:
  bool (*trigger_)();
  EMB_MILLISECONDS delay_;
  void (*action_)();

  bool trigger_detected_;
  EMB_MILLISECONDS trigger_timepoint_;
public:
  triggered_action(bool (*trigger)(), EMB_MILLISECONDS delay, void (*action)())
      : trigger_(trigger),
        delay_(delay),
        action_(action),
        trigger_detected_(false),
        trigger_timepoint_(emb::chrono::steady_clock::now()) {
    check_and_execute();
  }

  void check_and_execute() {
    if (trigger_()) {
      EMB_MILLISECONDS now = emb::chrono::steady_clock::now();
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

} // namespace chrono
} // namespace emb
