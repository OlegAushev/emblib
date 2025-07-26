#pragma once

#include <emblib/core.hpp>

namespace emb {

#ifdef __cpp_concepts

template<typename T, size_t Capacity = 0>
class circular_buffer {
private:
  T stack_data_[Capacity]{};
  T* data_;
  size_t const capacity_;
  size_t front_;
  size_t back_;
  bool full_;
public:
  constexpr circular_buffer()
    requires(Capacity > 0)
      : data_{stack_data_},
        capacity_{Capacity},
        front_{0},
        back_{0},
        full_{false} {}

  circular_buffer(size_t capacity)
    requires(Capacity == 0)
      : data_(new T[capacity]),
        capacity_{capacity},
        front_{0},
        back_{0},
        full_{false} {}

  constexpr ~circular_buffer() requires(Capacity > 0) = default;

  ~circular_buffer() requires(Capacity == 0) { delete[] data_; }

  constexpr void clear() {
    front_ = 0;
    back_ = 0;
    full_ = false;
  }

  constexpr bool empty() const { return (!full_ && (front_ == back_)); }

  constexpr bool full() const { return full_; }

  constexpr size_t capacity() const { return capacity_; }

  constexpr size_t size() const {
    size_t size{capacity_};
    if (!full_) {
      if (back_ >= front_) {
        size = back_ - front_;
      } else {
        size = capacity_ + back_ - front_;
      }
    }
    return size;
  }

  constexpr void push_back(T const& value) {
    data_[back_] = value;
    if (full_) {
      front_ = (front_ + 1) % capacity_;
    }
    back_ = (back_ + 1) % capacity_;
    full_ = (front_ == back_);
  }

  constexpr T& front() {
    assert(!empty());
    return data_[front_];
  }

  constexpr T const& front() const {
    assert(!empty());
    return data_[front_];
  }

  constexpr T& back() {
    assert(!empty());
    return data_[(back_ + capacity_ - 1) % capacity_];
  }

  constexpr T const& back() const {
    assert(!empty());
    return data_[(back_ + capacity_ - 1) % capacity_];
  }

  constexpr void pop_front() {
    assert(!empty());
    full_ = false;
    front_ = (front_ + 1) % capacity_;
  }

  constexpr T const* data() const { return data_; }

  constexpr T const* begin() const { return data_; }

  constexpr T const* end() const { return data_ + capacity_; }

  constexpr void fill(T const& value) {
    clear();
    for (size_t i{0}; i < capacity_; ++i) {
      data_[i] = value;
    }
    full_ = true;
  }
};

#else

template<typename T, size_t Capacity>
class circular_buffer {
private:
  T data_[Capacity];
  size_t front_;
  size_t back_;
  bool full_;
public:
  EMB_CONSTEXPR circular_buffer() : front_(0), back_(0), full_(false) {}

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

#endif

} // namespace emb
