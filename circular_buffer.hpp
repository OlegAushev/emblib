#pragma once

#include <emblib/core.hpp>

namespace emb {

template<typename T, size_t Capacity>
class static_circular_buffer {
private:
#ifdef __cpp_nsdmi
  T data_[Capacity]{};
#else
  T data_[Capacity];
#endif
  size_t front_;
  size_t back_;
  bool full_;
public:
  EMB_CONSTEXPR static_circular_buffer() : front_(0), back_(0), full_(false) {}

  EMB_CONSTEXPR void clear() {
    front_ = 0;
    back_ = 0;
    full_ = false;
  }

  EMB_CONSTEXPR bool empty() const { return (!full_ && (front_ == back_)); }

  EMB_CONSTEXPR bool full() const { return full_; }

  EMB_CONSTEXPR size_t capacity() const { return Capacity; }

  EMB_CONSTEXPR size_t size() const {
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

  EMB_CONSTEXPR void push_back(T const& value) {
    data_[back_] = value;
    if (full_) {
      front_ = (front_ + 1) % Capacity;
    }
    back_ = (back_ + 1) % Capacity;
    full_ = (front_ == back_);
  }

  EMB_CONSTEXPR T& front() {
    assert(!empty());
    return data_[front_];
  }

  EMB_CONSTEXPR T const& front() const {
    assert(!empty());
    return data_[front_];
  }

  EMB_CONSTEXPR T& back() {
    assert(!empty());
    return data_[(back_ + Capacity - 1) % Capacity];
  }

  EMB_CONSTEXPR T const& back() const {
    assert(!empty());
    return data_[(back_ + Capacity - 1) % Capacity];
  }

  EMB_CONSTEXPR void pop_front() {
    assert(!empty());
    full_ = false;
    front_ = (front_ + 1) % Capacity;
  }

  EMB_CONSTEXPR T const* data() const { return data_; }

  EMB_CONSTEXPR T const* begin() const { return data_; }

  EMB_CONSTEXPR T const* end() const { return data_ + Capacity; }

  EMB_CONSTEXPR void fill(T const& value) {
    clear();
    for (size_t i = 0; i < Capacity; ++i) {
      data_[i] = value;
    }
    full_ = true;
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
  explicit circular_buffer(size_t capacity)
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

  void pop_front() {
    assert(!empty());
    full_ = false;
    front_ = (front_ + 1) % Capacity;
  }

  T const* data() const { return data_; }

  T const* begin() const { return data_; }

  T const* end() const { return data_ + Capacity; }

  void fill(T const& value) {
    clear();
    for (size_t i = 0; i < Capacity; ++i) {
      data_[i] = value;
    }
    full_ = true;
  }
};

} // namespace emb
