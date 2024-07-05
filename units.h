#pragma once


#include <emblib/core.h>


namespace emb {
namespace units {


enum class angle_type {
    electrical,
    mechanical
};


template<angle_type T>
class rad {
private:
    const float _value;
public:
    explicit rad(float value) : _value(value) {}
    float get() const { return _value; }
};


template<angle_type T>
class deg {
private:
    const float _value;
public:
    explicit deg(float value) : _value(value) {}
    float get() const { return _value; }
};


template<angle_type T>
class radps {
private:
    const float _value;
public:
    explicit radps(float value) : _value(value) {}
    float get() const { return _value; }
};


class rpm {
private:
    const float _value;
public:
    explicit rpm(float value) : _value(value) {}
    float get() const { return _value; }
};


} // namespace units
} // namespace emb
