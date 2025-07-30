#pragma once

#include <emb/algorithm.hpp>
#include <emb/core.hpp>

namespace emb {

template<typename T, size_t Capacity>
class static_vector {
private:
  T data_[Capacity];
  size_t size_;
public:
  static_vector() : size_(0) {}

  explicit static_vector(size_t size) : size_(size) {
    assert(size <= Capacity);
    std::fill(begin(), end(), T());
  }

  static_vector(size_t size, T const& value) : size_(size) {
    assert(size <= Capacity);
    std::fill(begin(), end(), value);
  }

  template<class V>
  static_vector(V* first, V* last) : size_(size_t(last - first)) {
    assert(first <= last);
    assert(size_t(last - first) <= Capacity);
    std::copy(first, last, begin());
  }
public:
  size_t capacity() const { return Capacity; }

  size_t size() const { return size_; }

  bool empty() const { return size_ == 0; }

  bool full() const { return size_ == Capacity; }

  T& operator[](size_t pos) {
#ifdef NDEBUG
    return data_[pos];
#else
    return at(pos);
#endif
  }

  T const& operator[](size_t pos) const {
#ifdef NDEBUG
    return data_[pos];
#else
    return at(pos);
#endif
  }

  T& at(size_t pos) {
    assert(pos < size_);
    return data_[pos];
  }

  T const& at(size_t pos) const {
    assert(pos < size_);
    return data_[pos];
  }
public:
  T* begin() { return data_; }

  T* end() { return data_ + size_; }

  T const* begin() const { return data_; }

  T const* end() const { return data_ + size_; }

  T* data() { return data_; }

  T const* data() const { return data_; }

  T& front() {
    assert(!empty());
    return data_[0];
  }

  T const& front() const {
    assert(!empty());
    return data_[0];
  }

  T& back() {
    assert(!empty());
    return data_[size_ - 1];
  }

  T const& back() const {
    assert(!empty());
    return data_[size_ - 1];
  }
public:
  void resize(size_t size) {
    assert(size <= Capacity);
    if (size > size_) {
      std::fill(data_ + size_, data_ + size, T());
    }
    size_ = size;
  }

  void resize(size_t size, T const& value) {
    assert(size <= Capacity);
    if (size > size_) {
      std::fill(data_ + size_, data_ + size, value);
    }
    size_ = size;
  }

  void clear() { size_ = 0; }
public:
  void push_back(T const& value) {
    assert(!full());
    data_[size_++] = value;
  }

  void pop_back() {
    assert(!empty());
    --size_;
  }

  void insert(T* pos, T const& value) {
    assert(!full());
    assert(pos <= end());

    static_vector<T, Capacity> buf(pos, end());
    *pos++ = value;
    std::copy(buf.begin(), buf.end(), pos);
    ++size_;
  }

  void insert(T* pos, size_t count, T const& value) {
    assert((size_ + count) <= Capacity);
    assert(pos <= end());

    static_vector<T, Capacity> buf(pos, end());
    for (size_t i = 0; i < count; ++i) {
      *pos++ = value;
    }
    std::copy(buf.begin(), buf.end(), pos);
    size_ += count;
  }

  template<class V>
  void insert(T* pos, V* first, V* last) {
    assert(first <= last);
    assert(size_t(size_ + last - first) <= Capacity);
    assert(pos <= end());

    static_vector<T, Capacity> buf(pos, end());
    pos = std::copy(first, last, pos);
    std::copy(buf.begin(), buf.end(), pos);
    size_ = size_ + size_t(last - first);
  }
public:
  T* erase(T* pos) {
    assert(!empty());
    assert(pos < end());

    static_vector<T, Capacity> buf(pos + 1, end());
    --size_;
    return std::copy(buf.begin(), buf.end(), pos);
  }

  T* erase(T* first, T* last) {
    assert(first <= last);
    assert(size_ >= size_t(last - first));
    assert(begin() <= first);
    assert(last <= end());

    static_vector<T, Capacity> buf(last, end());
    size_ = size_ - size_t(last - first);
    return std::copy(buf.begin(), buf.end(), first);
  }
};

} // namespace emb
