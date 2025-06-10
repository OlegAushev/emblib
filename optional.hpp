#pragma once

#include <emblib/core.hpp>
#include <new>
#if __cplusplus >= 201700
#include <optional>
#endif

namespace emb {

#if __cplusplus >= 201700

template<typename T>
using optional = std::optional<T>;

using nullopt_t = std::nullopt_t;

nullopt_t const nullopt = std::nullopt;

#else

struct nullopt_t {
  explicit nullopt_t() {}
};

nullopt_t const nullopt;

template<class T>
class optional {
private:
  char _storage[sizeof(T)];
  bool _has_value;
public:
  optional() : _has_value(false) {}

  optional(nullopt_t) : _has_value(false) {}

  optional(T const& value) {
    new (_storage) T(value);
    _has_value = true;
  }

  optional(optional<T> const& other) {
    if (other._has_value) {
      new (_storage) T(other.value());
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

  T const& value() const { return *(reinterpret_cast<T const*>(_storage)); }

  T& value() { return *(reinterpret_cast<T*>(_storage)); }

  T const* operator->() const { return reinterpret_cast<T const*>(_storage); }

  T* operator->() { return reinterpret_cast<T*>(_storage); }

  T const& operator*() const { return *(reinterpret_cast<T const*>(_storage)); }

  T& operator*() { return *(reinterpret_cast<T*>(_storage)); }

  T value_or(T const& default_value) const {
    return _has_value ? value() : default_value;
  }

  optional& operator=(T const& value) {
    reset();
    new (_storage) T(value);
    _has_value = true;
    return *this;
  }

  optional& operator=(optional<T> const& other) {
    reset();
    if (other._has_value) {
      new (_storage) T(other.value());
    }
    _has_value = other._has_value;
    return *this;
  }
};

#endif

} // namespace emb
