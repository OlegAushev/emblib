#pragma once

#include <emblib/core.hpp>
#include <new>
#if __cplusplus >= 201700
#include <optional>
#endif

namespace emb {

#if __cplusplus >= 201700

template <typename T>
using optional = std::optional<T>;

using nullopt_t = std::nullopt_t;

const nullopt_t nullopt = std::nullopt;

#else

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

    optional(const optional<T>& other) {
        if (other._has_value) {
            new(_storage) T(other.value());
        }
        _has_value = other._has_value;
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

    explicit operator bool() const { return _has_value; }
    bool has_value() const { return _has_value; }

    const T& value() const  { return *(reinterpret_cast<const T*>(_storage)); }
    T& value() { return *(reinterpret_cast<T*>(_storage)); }

    const T* operator->() const { return reinterpret_cast<const T*>(_storage); }
    T* operator->() { return reinterpret_cast<T*>(_storage); }

    const T& operator*() const {
        return *(reinterpret_cast<const T*>(_storage));
    }
    T& operator*() { return *(reinterpret_cast<T*>(_storage)); }

    T value_or(const T& default_value) const {
        return _has_value ? value() : default_value;
    }

    optional& operator=(const T& value) {
        reset();
        new(_storage) T(value);
        _has_value = true;
        return *this;
    }

    optional& operator=(const optional<T>& other) {
        reset();
        if (other._has_value) {
            new(_storage) T(other.value());
        }
        _has_value = other._has_value;
        return *this;
    }
};

#endif

} // namespace emb
