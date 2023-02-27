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
	int64_t _value;
public:
	Duration() : _value(0) {}
	explicit Duration(int64_t value) : _value(value) {}
	Duration(const Duration& other) { this->_value = other._value; }
	//Duration(const volatile Duration& other) { this->_value = other._value; }
	Duration& operator=(const Duration& other)
	{
		if (this != &other)
		{
			this->_value = other._value;
		}
		return *this;
	}
};




} // namespace impl


typedef impl::Duration<1> nanoseconds;
typedef impl::Duration<1000> microseconds;
typedef impl::Duration<1000000> milliseconds;
typedef impl::Duration<1000000000> seconds;

} // namespace chrono

} // namespace emb

