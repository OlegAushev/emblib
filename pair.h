#pragma once


#include <emblib_c28x/core.h>


namespace emb {

template <typename T1, typename T2>
class pair {
public:
    T1 first;
    T2 second;
    pair() {}
    pair(const T1& first_, const T2& second_)
        : first(first_)
        , second(second_) {}

    pair& operator=(const pair& other) {
        if (this != &other)	{
            // not a self-assignment
            this->first = other.first;
            this->second = other.second;
        }
        return *this;
    }
};

} // namespace emb

