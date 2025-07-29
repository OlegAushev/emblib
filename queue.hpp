#pragma once

#include <emblib/core.hpp>

#ifdef __cpp_concepts
#define EMB_QUEUE_V2
#endif

#ifdef EMB_QUEUE_V2
#include <array>
#include <type_traits>
#include <vector>
#endif

namespace emb {

#ifdef EMB_QUEUE_V2

inline namespace v2 {

template<typename T, size_t Capacity = 0>
class queue {
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
  size_type size_;
public:
  constexpr queue()
    requires(Capacity > 0)
      : data_{}, capacity_(Capacity), front_{0}, back_{0}, size_{0} {}

  constexpr explicit queue(size_type capacity)
    requires(Capacity == 0)
      : data_(capacity), capacity_{capacity}, front_{0}, back_{0}, size_{0} {}

  constexpr void clear() {
    front_ = 0;
    back_ = 0;
    size_ = 0;
  }

  constexpr bool empty() const { return size_ == 0; }

  constexpr bool full() const { return size_ == capacity_; }

  constexpr size_type capacity() const { return capacity_; }

  constexpr size_type size() const { return size_; }

  constexpr void push(value_type const& value) {
    assert(!full());
    if (empty()) {
      back_ = front_;
    } else {
      back_ = (back_ + 1) % capacity_;
    }
    data_[back_] = value;
    ++size_;
  }

  constexpr const_reference front() const {
    assert(!empty());
    return data_[front_];
  }

  constexpr const_reference back() const {
    assert(!empty());
    return data_[back_];
  }

  constexpr void pop() {
    assert(!empty());
    front_ = (front_ + 1) % capacity_;
    --size_;
  }
};

} // namespace v2

namespace v1 {

#endif

template<typename T, size_t Capacity>
class queue {
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
  size_type size_;
public:
  EMB_CONSTEXPR queue() : front_(0), back_(0), size_(0) {}

  EMB_CONSTEXPR void clear() {
    front_ = 0;
    back_ = 0;
    size_ = 0;
  }

  EMB_CONSTEXPR bool empty() const { return size_ == 0; }

  EMB_CONSTEXPR bool full() const { return size_ == Capacity; }

  EMB_CONSTEXPR size_type capacity() const { return Capacity; }

  EMB_CONSTEXPR size_type size() const { return size_; }

  EMB_CONSTEXPR void push(value_type const& value) {
    assert(!full());
    if (empty()) {
      back_ = front_;
    } else {
      back_ = (back_ + 1) % Capacity;
    }
    data_[back_] = value;
    ++size_;
  }

  EMB_CONSTEXPR const_reference front() const {
    assert(!empty());
    return data_[front_];
  }

  EMB_CONSTEXPR const_reference back() const {
    assert(!empty());
    return data_[back_];
  }

  EMB_CONSTEXPR void pop() {
    assert(!empty());
    front_ = (front_ + 1) % Capacity;
    --size_;
  }
};

#ifdef EMB_QUEUE_V2

} // namespace v1

template<typename T>
struct is_queue : std::false_type {};

template<typename T, size_t Size>
struct is_queue<emb::queue<T, Size>> : std::true_type {};

template<typename T, size_t Size>
struct is_queue<emb::v1::queue<T, Size>> : std::true_type {};

template<typename T>
concept Queue = is_queue<T>::value;

#endif

} // namespace emb
