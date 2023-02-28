#pragma once


#include "emb_core.h"


namespace emb {

namespace chrono {

namespace impl {

template <int64_t Divider>
class Duration
{
public:
	static const int64_t divider = Divider;
private:
	int64_t _ticks;
public:
	Duration() : _ticks(0) {}
	explicit Duration(int64_t tick_count) : _ticks(tick_count) {}
	Duration(const Duration& other) { this->_ticks = other._ticks; }
	Duration(const volatile Duration& other) { this->_ticks = other._ticks; }

	Duration& operator=(const Duration& other)
	{
		if (this != &other)
		{
			this->_ticks = other._ticks;
		}
		return *this;
	}

	volatile Duration& operator=(const Duration& other) volatile
	{
		if (this != &other)
		{
			this->_ticks = other._ticks;
		}
		return *this;
	}

	Duration& operator+(const Duration& other)
	{
		this->_ticks += other._ticks;
		return *this;
	}

	Duration& operator-(const Duration& other)
	{
		this->_ticks -= other._ticks;
		return *this;
	}

	bool operator>(const Duration& other) const { return this->_ticks > other._ticks; }
	bool operator>=(const Duration& other) const { return this->_ticks >= other._ticks; }
	bool operator<(const Duration& other) const { return this->_ticks < other._ticks; }
	bool operator<=(const Duration& other) const { return this->_ticks <= other._ticks; }
	bool operator==(const Duration& other) const { return this->_ticks == other._ticks; }
	bool operator!=(const Duration& other) const { return this->_ticks != other._ticks; }

	int64_t count() const { return _ticks; }
};

} // namespace impl


template<typename ToDuration, int64_t Divider>
ToDuration duration_cast(const impl::Duration<Divider> duration)
{
	return impl::Duration<ToDuration::divider>(duration.count() * Divider / ToDuration::divider);
}


typedef impl::Duration<1> nanoseconds;
typedef impl::Duration<1000> microseconds;
typedef impl::Duration<1000000> milliseconds;
typedef impl::Duration<1000000000> seconds;

} // namespace chrono

} // namespace emb

