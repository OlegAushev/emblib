#pragma once


#include <c28x_emblib/core.h>


namespace emb {

template <typename T1, typename T2>
class Pair
{
public:
	T1 first;
	T2 second;
	Pair() {}
	Pair(const T1& first_, const T2& second_)
		: first(first_)
		, second(second_) {}

	Pair& operator=(const Pair& other)
	{
		if (this != &other)	// not a self-assignment
		{
			this->first = other.first;
			this->second = other.second;
		}
		return *this;
	}
};

} // namespace emb

