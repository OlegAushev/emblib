#pragma once


#include <emblib/core.h>
#include <new>


namespace emb {


struct nullopt_t {
    explicit nullopt_t() {}
};


const nullopt_t nullopt;


template <class T>
class optional {
private:
    char _storage[sizeof(T)];
    bool _has_value;
public:
    optional() : _has_value(false) {}
    optional(nullopt_t) : _has_value(false) {}
    optional(const T& value) {
        new(_storage) T(value);
        _has_value = true;
    }

    ~optional() {
        if (_has_value) {
            reinterpret_cast<T*>(_storage)->~T();
        }
    }

    void reset() {
        if (_has_value) {
            _has_value = false;
            reinterpret_cast<T*>(_storage)->~T();
        }
    }

    bool has_value() const { return _has_value; }
    const T& value() const { return *(reinterpret_cast<const T*>(_storage)); }
    T value_or(const T& default_value) const { return _has_value ? value() : default_value; }

    optional& operator=(const T& value) {
        reset();
        new(_storage) T(value);
        _has_value = true;
        return *this;
    }

    optional& operator=(const optional<T> other) {
        reset();
        if (other._has_value) {
            T value = other.value();
            new(_storage) T(value);
        }
        _has_value = other._has_value;
        return *this;
    }
};


} // namespace emb
