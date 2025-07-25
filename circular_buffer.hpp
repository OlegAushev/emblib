#pragma once

#include <emblib/core.hpp>

namespace emb {

template<typename T, size_t Capacity>
class static_circular_buffer {
private:
  T data_[Capacity];
  size_t front_;
  size_t back_;
  bool full_;
public:
  static_circular_buffer() : front_(0), back_(0), full_(false) {}

  void clear() {
    front_ = 0;
    back_ = 0;
    full_ = false;
  }

  bool empty() const { return (!full_ && (front_ == back_)); }

  bool full() const { return full_; }

  size_t capacity() const { return Capacity; }

  size_t size() const {
    size_t size = Capacity;
    if (!full_) {
      if (back_ >= front_) {
        size = back_ - front_;
      } else {
        size = Capacity + back_ - front_;
      }
    }
    return size;
  }

  void push_back(T const& value) {
    data_[back_] = value;
    if (full_) {
      front_ = (front_ + 1) % Capacity;
    }
    back_ = (back_ + 1) % Capacity;
    full_ = (front_ == back_);
  }

  T& front() {
    assert(!empty());
    return data_[front_];
  }

  T const& front() const {
    assert(!empty());
    return data_[front_];
  }

  T& back() {
    assert(!empty());
    return data_[(back_ + Capacity - 1) % Capacity];
  }

  T const& back() const {
    assert(!empty());
    return data_[(back_ + Capacity - 1) % Capacity];
  }

  void pop() {
    assert(!empty());
    full_ = false;
    front_ = (front_ + 1) % Capacity;
  }

  T const* data() const { return data_; }

  T const* begin() const { return data_; }

  T const* end() const { return data_ + Capacity; }

  void fill(T const& value) {
    for (size_t i = 0; i < Capacity; ++i) {
      data_[i] = value;
    }
  }
};

template<typename T>
class circular_buffer {
private:
  T* data_;
  size_t Capacity;
  size_t front_;
  size_t back_;
  bool full_;
public:
  circular_buffer(size_t capacity)
      : data_(new T[capacity]),
        Capacity(capacity),
        front_(0),
        back_(0),
        full_(false) {}

  ~circular_buffer() { delete[] data_; }

  void clear() {
    front_ = 0;
    back_ = 0;
    full_ = false;
  }

  bool empty() const { return (!full_ && (front_ == back_)); }

  bool full() const { return full_; }

  size_t capacity() const { return Capacity; }

  size_t size() const {
    size_t size = Capacity;
    if (!full_) {
      if (back_ >= front_) {
        size = back_ - front_;
      } else {
        size = Capacity + back_ - front_;
      }
    }
    return size;
  }

  void push_back(T const& value) {
    data_[back_] = value;
    if (full_) {
      front_ = (front_ + 1) % Capacity;
    }
    back_ = (back_ + 1) % Capacity;
    full_ = (front_ == back_);
  }

  T& front() {
    assert(!empty());
    return data_[front_];
  }

  T const& front() const {
    assert(!empty());
    return data_[front_];
  }

  T& back() {
    assert(!empty());
    return data_[(back_ + Capacity - 1) % Capacity];
  }

  T const& back() const {
    assert(!empty());
    return data_[(back_ + Capacity - 1) % Capacity];
  }

  void pop() {
    assert(!empty());
    full_ = false;
    front_ = (front_ + 1) % Capacity;
  }

  T const* data() const { return data_; }

  T const* begin() const { return data_; }

  T const* end() const { return data_ + Capacity; }

  void fill(T const& value) {
    for (size_t i = 0; i < Capacity; ++i) {
      data_[i] = value;
    }
  }
};

} // namespace emb
