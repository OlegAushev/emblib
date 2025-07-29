#pragma once

#include <emblib/core.hpp>

#ifdef __cpp_concepts
#define EMB_CIRCULAR_BUFFER_V2
#endif

#ifdef EMB_CIRCULAR_BUFFER_V2
#include <array>
#include <type_traits>
#include <vector>
#endif

namespace emb {

#ifdef EMB_CIRCULAR_BUFFER_V2

inline namespace v2 {

template<typename T, size_t Capacity = 0>
class circular_buffer {
public:
  using value_type = T;
  using size_type = size_t;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = value_type*;
  using const_pointer = value_type const*;
  using underlying_type = std::conditional_t<
      (Capacity > 0),
      std::array<value_type, Capacity>,
      std::vector<value_type>>;
private:
  underlying_type data_;
  size_type const capacity_;
  size_type front_;
  size_type back_;
  bool full_;
public:
  constexpr circular_buffer()
    requires(Capacity > 0)
      : data_{}, capacity_{Capacity}, front_{0}, back_{0}, full_{false} {}

  constexpr explicit circular_buffer(size_type capacity)
    requires(Capacity == 0)
      : data_(capacity),
        capacity_{capacity},
        front_{0},
        back_{0},
        full_{false} {}

  constexpr void clear() {
    front_ = 0;
    back_ = 0;
    full_ = false;
  }

  constexpr bool empty() const { return (!full_ && (front_ == back_)); }

  constexpr bool full() const { return full_; }

  constexpr size_type capacity() const { return capacity_; }

  constexpr size_type size() const {
    size_type size{capacity_};
    if (!full_) {
      if (back_ >= front_) {
        size = back_ - front_;
      } else {
        size = capacity_ + back_ - front_;
      }
    }
    return size;
  }

  constexpr void push_back(value_type const& value) {
    data_[back_] = value;
    if (full_) {
      front_ = (front_ + 1) % capacity_;
    }
    back_ = (back_ + 1) % capacity_;
    full_ = (front_ == back_);
  }

  constexpr reference front() {
    assert(!empty());
    return data_[front_];
  }

  constexpr const_reference front() const {
    assert(!empty());
    return data_[front_];
  }

  constexpr reference back() {
    assert(!empty());
    return data_[(back_ + capacity_ - 1) % capacity_];
  }

  constexpr const_reference back() const {
    assert(!empty());
    return data_[(back_ + capacity_ - 1) % capacity_];
  }

  constexpr void pop_front() {
    assert(!empty());
    full_ = false;
    front_ = (front_ + 1) % capacity_;
  }

  constexpr void pop_back() {
    assert(!empty());
    full_ = false;
    back_ = (back_ + capacity_ - 1) % capacity_;
  }

  constexpr const_pointer data() const { return data_.data(); }

  constexpr const_pointer begin() const { return data_.begin(); }

  constexpr const_pointer end() const { return data_.end(); }

  constexpr void fill(value_type const& value) {
    clear();
    std::fill(data_.begin(), data_.end(), value);
    full_ = true;
  }
};

} // namespace v2

namespace v1 {

#endif

template<typename T, size_t Capacity>
class circular_buffer {
public:
  typedef T value_type;
  typedef size_t size_type;
  typedef value_type& reference;
  typedef value_type const& const_reference;
  typedef value_type* pointer;
  typedef value_type const* const_pointer;
private:
  value_type data_[Capacity]
#if __cplusplus >= 201100
      {}
#endif
  ;
  size_type front_;
  size_type back_;
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

  EMB_CONSTEXPR size_type capacity() const { return Capacity; }

  EMB_CONSTEXPR size_type size() const {
    size_type size = Capacity;
    if (!full_) {
      if (back_ >= front_) {
        size = back_ - front_;
      } else {
        size = Capacity + back_ - front_;
      }
    }
    return size;
  }

  EMB_CONSTEXPR void push_back(value_type const& value) {
    data_[back_] = value;
    if (full_) {
      front_ = (front_ + 1) % Capacity;
    }
    back_ = (back_ + 1) % Capacity;
    full_ = (front_ == back_);
  }

  EMB_CONSTEXPR reference front() {
    assert(!empty());
    return data_[front_];
  }

  EMB_CONSTEXPR const_reference front() const {
    assert(!empty());
    return data_[front_];
  }

  EMB_CONSTEXPR reference back() {
    assert(!empty());
    return data_[(back_ + Capacity - 1) % Capacity];
  }

  EMB_CONSTEXPR const_reference back() const {
    assert(!empty());
    return data_[(back_ + Capacity - 1) % Capacity];
  }

  EMB_CONSTEXPR void pop_front() {
    assert(!empty());
    full_ = false;
    front_ = (front_ + 1) % Capacity;
  }

  EMB_CONSTEXPR void pop_back() {
    assert(!empty());
    full_ = false;
    back_ = (back_ + Capacity - 1) % Capacity;
  }

  EMB_CONSTEXPR const_pointer data() const { return data_; }

  EMB_CONSTEXPR const_pointer begin() const { return data_; }

  EMB_CONSTEXPR const_pointer end() const { return data_ + Capacity; }

  EMB_CONSTEXPR void fill(value_type const& value) {
    clear();
    for (size_t i = 0; i < Capacity; ++i) {
      data_[i] = value;
    }
    full_ = true;
  }
};

#ifdef EMB_CIRCULAR_BUFFER_V2

} // namespace v1

template<typename T>
struct is_circular_buffer : std::false_type {};

template<typename T, size_t Size>
struct is_circular_buffer<emb::circular_buffer<T, Size>> : std::true_type {};

template<typename T, size_t Size>
struct is_circular_buffer<emb::v1::circular_buffer<T, Size>> : std::true_type {};

template<typename T>
concept CircularBuffer = is_circular_buffer<T>::value;

#endif

} // namespace emb
