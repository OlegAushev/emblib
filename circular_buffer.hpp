#pragma once

#include <emblib/core.hpp>

namespace emb {

template<typename T, size_t Capacity>
class circular_buffer {
private:
  T _data[Capacity];
  size_t _front;
  size_t _back;
  bool _full;
public:
  circular_buffer() : _front(0), _back(0), _full(false) {}

  void clear() {
    _front = 0;
    _back = 0;
    _full = false;
  }

  bool empty() const { return (!_full && (_front == _back)); }

  bool full() const { return _full; }

  size_t capacity() const { return Capacity; }

  size_t size() const {
    size_t size = Capacity;
    if (!_full) {
      if (_back >= _front) {
        size = _back - _front;
      } else {
        size = Capacity + _back - _front;
      }
    }
    return size;
  }

  void push_back(T const& value) {
    _data[_back] = value;
    if (_full) {
      _front = (_front + 1) % Capacity;
    }
    _back = (_back + 1) % Capacity;
    _full = (_front == _back);
  }

  T& front() {
    assert(!empty());
    return _data[_front];
  }

  T const& front() const {
    assert(!empty());
    return _data[_front];
  }

  T& back() {
    assert(!empty());
    return _data[(_back + Capacity - 1) % Capacity];
  }

  T const& back() const {
    assert(!empty());
    return _data[(_back + Capacity - 1) % Capacity];
  }

  void pop() {
    assert(!empty());
    _full = false;
    _front = (_front + 1) % Capacity;
  }

  T const* data() const { return _data; }

  T const* begin() const { return _data; }

  T const* end() const { return _data + Capacity; }

  void fill(T const& value) {
    for (size_t i = 0; i < Capacity; ++i) {
      _data[i] = value;
    }
  }
};

} // namespace emb
