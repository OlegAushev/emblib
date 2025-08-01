#pragma once

#include <emb/core.hpp>

#ifdef __cpp_concepts
#define EMB_STACK_V2
#endif

#ifdef EMB_STACK_V2
#include <array>
#include <type_traits>
#include <vector>
#endif

namespace emb {

#ifdef EMB_STACK_V2

inline namespace v2 {

template<typename T, size_t Capacity = 0>
class stack {
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
  size_type size_;
public:
  constexpr stack()
    requires(Capacity > 0)
      : data_{}, capacity_{Capacity}, size_{0} {}

  constexpr stack(size_type capacity)
    requires(Capacity == 0)
      : data_(capacity), capacity_{capacity}, size_{0} {}

  constexpr void clear() { size_ = 0; }

  constexpr bool empty() const { return size_ == 0; }

  constexpr bool full() const { return size_ == capacity_; }

  constexpr size_type capacity() const { return capacity_; }

  constexpr size_type size() const { return size_; }

  constexpr void push(value_type const& value) {
    assert(!full());
    data_[size_] = value;
    ++size_;
  }

  constexpr const_reference top() const {
    assert(!empty());
    return data_[size_ - 1];
  }

  constexpr void pop() {
    assert(!empty());
    --size_;
  }
};

} // namespace v2

namespace v1 {

#endif

template<typename T, size_t Capacity>
class stack {
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
  size_type size_;
public:
  EMB_CONSTEXPR stack() : size_(0) {}

  EMB_CONSTEXPR void clear() { size_ = 0; }

  EMB_CONSTEXPR bool empty() const { return size_ == 0; }

  EMB_CONSTEXPR bool full() const { return size_ == Capacity; }

  EMB_CONSTEXPR size_type capacity() const { return Capacity; }

  EMB_CONSTEXPR size_type size() const { return size_; }

  EMB_CONSTEXPR void push(value_type const& value) {
    assert(!full());
    data_[size_] = value;
    ++size_;
  }

  EMB_CONSTEXPR const_reference top() const {
    assert(!empty());
    return data_[size_ - 1];
  }

  EMB_CONSTEXPR void pop() {
    assert(!empty());
    --size_;
  }
};

#ifdef EMB_STACK_V2

} // namespace v1

template<typename T>
struct is_stack : std::false_type {};

template<typename T, size_t Size>
struct is_stack<emb::stack<T, Size>> : std::true_type {};

template<typename T, size_t Size>
struct is_stack<emb::v1::stack<T, Size>> : std::true_type {};

template<typename T>
concept Stack = is_stack<T>::value;

#endif

} // namespace emb
