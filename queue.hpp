#pragma once

#include <emblib/core.hpp>

namespace emb {

template<typename T, size_t Capacity>
class queue {
private:
  T data_[Capacity];
  size_t front_;
  size_t back_;
  size_t size_;
public:
  queue() : front_(0), back_(0), size_(0) {}

  void clear() {
    front_ = 0;
    back_ = 0;
    size_ = 0;
  }

  bool empty() const { return size_ == 0; }

  bool full() const { return size_ == Capacity; }

  size_t capacity() const { return Capacity; }

  size_t size() const { return size_; }

  void push(T const& value) {
    assert(!full());

    if (empty()) {
      back_ = front_;
    } else {
      back_ = (back_ + 1) % Capacity;
    }
    data_[back_] = value;
    ++size_;
  }

  T const& front() const {
    assert(!empty());
    return data_[front_];
  }

  T const& back() const {
    assert(!empty());
    return data_[back_];
  }

  void pop() {
    assert(!empty());
    front_ = (front_ + 1) % Capacity;
    --size_;
  }
};

} // namespace emb
