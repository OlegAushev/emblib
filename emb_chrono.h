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
	int64_t _nanoseconds;
public:
	Duration() : _nanoseconds(0) {}
	explicit Duration(int64_t value) : _nanoseconds(value * Divider) {}
	Duration(const Duration& other) { this->_nanoseconds = other._nanoseconds; }
	Duration(const volatile Duration& other) { this->_nanoseconds = other._nanoseconds; }

	Duration& operator=(const Duration& other)
	{
		if (this != &other)
		{
			this->_nanoseconds = other._nanoseconds;
		}
		return *this;
	}

	volatile Duration& operator=(const Duration& other) volatile
	{
		if (this != &other)
		{
			this->_nanoseconds = other._nanoseconds;
		}
		return *this;
	}

	Duration& operator+(const Duration& other)
	{
		this->_nanoseconds += other._nanoseconds;
		return *this;
	}

	Duration& operator-(const Duration& other)
	{
		this->_nanoseconds -= other._nanoseconds;
		return *this;
	}

	bool operator>(const Duration& other) const { return this->_nanoseconds > other._nanoseconds; }
	bool operator>=(const Duration& other) const { return this->_nanoseconds >= other._nanoseconds; }
	bool operator<(const Duration& other) const { return this->_nanoseconds < other._nanoseconds; }
	bool operator<=(const Duration& other) const { return this->_nanoseconds <= other._nanoseconds; }
	bool operator==(const Duration& other) const { return this->_nanoseconds == other._nanoseconds; }
	bool operator!=(const Duration& other) const { return this->_nanoseconds != other._nanoseconds; }

	int64_t get() const { return _nanoseconds / Divider; }
};

} // namespace impl


typedef impl::Duration<1> nanoseconds;
typedef impl::Duration<1000> microseconds;
typedef impl::Duration<1000000> milliseconds;
typedef impl::Duration<1000000000> seconds;

} // namespace chrono

} // namespace emb

