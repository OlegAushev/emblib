#pragma once


#include <emblib/core.h>
#if defined(EMBLIB_ARM)
#include <chrono>
#endif


namespace emb {


namespace chrono {


#if defined(EMBLIB_C28X)


namespace impl {


template <int64_t Divider>
class duration {
public:
    static const int64_t divider = Divider;
private:
    int64_t _ticks;
public:
    duration() : _ticks(0) {}
    explicit duration(int64_t tick_count) : _ticks(tick_count) {}
    duration(const duration& other) { this->_ticks = other._ticks; }
    duration& operator=(const duration& other) {
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
};


template <int64_t Divider>
duration<Divider> operator+(const duration<Divider>& lhs, const duration<Divider>& rhs) {
    return duration<Divider>(lhs.count() + rhs.count());
}


template <int64_t Divider>
duration<Divider> operator-(const duration<Divider>& lhs, const duration<Divider>& rhs) {
    return duration<Divider>(lhs.count() - rhs.count());
}

template <int64_t Divider>
bool operator>(const duration<Divider>& lhs, const duration<Divider>& rhs) { return lhs.count() > rhs.count(); }
template <int64_t Divider>
bool operator>=(const duration<Divider>& lhs, const duration<Divider>& rhs) { return lhs.count() >= rhs.count(); }
template <int64_t Divider>
bool operator<(const duration<Divider>& lhs, const duration<Divider>& rhs) { return lhs.count() < rhs.count(); }
template <int64_t Divider>
bool operator<=(const duration<Divider>& lhs, const duration<Divider>& rhs) { return lhs.count() <= rhs.count(); }
template <int64_t Divider>
bool operator==(const duration<Divider>& lhs, const duration<Divider>& rhs) { return lhs.count() == rhs.count(); }
template <int64_t Divider>
bool operator!=(const duration<Divider>& lhs, const duration<Divider>& rhs) { return lhs.count() != rhs.count(); }


} // namespace impl


template<typename ToDuration, int64_t Divider>
ToDuration duration_cast(const impl::duration<Divider> duration) {
    return impl::duration<ToDuration::divider>(duration.count() * Divider / ToDuration::divider);
}


typedef impl::duration<1> nanoseconds;
typedef impl::duration<1000> microseconds;
typedef impl::duration<1000000> milliseconds;
typedef impl::duration<1000000000> seconds;


#endif


class steady_clock {
private:
    steady_clock();
    steady_clock(const steady_clock& other);
    steady_clock& operator=(const steady_clock& other);

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
            : _timeout(timeout)
            , _start(emb::chrono::steady_clock::now())
    {}

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


} // namespace chrono


} // namespace emb
