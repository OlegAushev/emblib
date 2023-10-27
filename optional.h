#pragma once


#include <emblib/core.h>


namespace emb {


template <class T>
class optional {
private:
    union {
        char _dummy;
        T _value;
    };
    bool _has_value;
public:
    optional() : _dummy(0), _has_value(false) {}
    optional(const T& value) : _value(value), _has_value(true) {}
    ~optional() {
        if (_has_value) {
            _value.~T();
        }
    }

    optional& operator=(const T& value) {
        _value = value;
        _has_value = true;
        return *this;
    }

    optional& operator=(const optional<T> other) {
        _has_value = other._has_value;
        if (_has_value) {
            _value = other._value;
        } else {
            _dummy = 0;
        }
        return *this;
    }

    bool has_value() const { return _has_value; }
    const T& value() const { return _value; }
    T value_or(const T& default_value) { return _has_value ? _value : default_value; }

    void reset() {
        if (_has_value) {
            _value.~T();
            _has_value = false;
        }
    }
};


} // namespace emb
