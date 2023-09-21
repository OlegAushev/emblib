#pragma once


#include <emblib/core.h>


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

    bool operator==(const pair& other) const {
        return (this->first == other.first) && (this->second == other.second);
    }

    bool operator!=(const pair& other) const {
        return !(*this == other);
    }
};


} // namespace emb
