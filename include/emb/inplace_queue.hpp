#pragma once

#include <array>
#include <cassert>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace emb {

template<typename T, std::size_t Capacity>
class inplace_queue;

template<typename T, size_t Capacity>
  requires std::is_default_constructible_v<T>
        && std::is_trivially_destructible_v<T>
        && (Capacity > 0)
class inplace_queue<T, Capacity> {
public:
  using value_type = T;
  using size_type = std::size_t;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = value_type*;
  using const_pointer = value_type const*;

private:
  std::array<value_type, Capacity> data_{};
  static constexpr size_type capacity_ = Capacity;
  size_type front_ = 0;
  size_type size_ = 0;

  constexpr size_type index_of(size_type offset) const {
    return (front_ + offset) % capacity_;
  }
public:
  constexpr inplace_queue() = default;
  constexpr inplace_queue(inplace_queue const&) = default;
  constexpr inplace_queue(inplace_queue&&) = default;
  constexpr inplace_queue& operator=(inplace_queue const&) = default;
  constexpr inplace_queue& operator=(inplace_queue&&) = default;
  constexpr ~inplace_queue() = default;

  constexpr void clear() {
    front_ = 0;
    size_ = 0;
  }

  [[nodiscard]] constexpr bool empty() const {
    return size_ == 0;
  }

  [[nodiscard]] constexpr bool full() const {
    return size_ == capacity_;
  }

  [[nodiscard]] constexpr size_type capacity() const {
    return capacity_;
  }

  [[nodiscard]] constexpr size_type size() const {
    return size_;
  }

  [[nodiscard]] constexpr const_reference front() const {
    assert(!empty());
    return data_[front_];
  }

  [[nodiscard]] constexpr const_reference back() const {
    assert(!empty());
    return data_[index_of(size_ - 1)];
  }

  constexpr void push(value_type const& value)
    requires std::is_copy_assignable_v<value_type> {
    assert(!full());
    data_[index_of(size_)] = value;
    ++size_;
  }

  constexpr void push(value_type&& value)
    requires std::is_move_assignable_v<value_type> {
    assert(!full());
    data_[index_of(size_)] = std::move(value);
    ++size_;
  }

  template<typename... Args>
  constexpr void emplace(Args&&... args)
    requires std::is_constructible_v<value_type, Args...> {
    assert(!full());
    std::construct_at(&data_[index_of(size_)], std::forward<Args>(args)...);
    ++size_;
  }

  [[nodiscard]] constexpr bool try_push(value_type const& value)
    requires std::is_copy_assignable_v<value_type> {
    if (full()) return false;
    push(value);
    return true;
  }

  [[nodiscard]] constexpr bool try_push(value_type&& value)
    requires std::is_move_assignable_v<value_type> {
    if (full()) return false;
    push(std::move(value));
    return true;
  }

  constexpr void pop() {
    assert(!empty());
    front_ = (front_ + 1) % capacity_;
    --size_;
  }

  [[nodiscard]] constexpr std::optional<value_type> try_pop() {
    if (empty()) return std::nullopt;
    std::optional<value_type> result{front()};
    pop();
    return result;
  }
};

template<typename T, size_t Capacity>
  requires(!std::is_default_constructible_v<T>
           || !std::is_trivially_destructible_v<T>)
       && (Capacity > 0)
class inplace_queue<T, Capacity> {
public:
  using value_type = T;
  using size_type = size_t;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = value_type*;
  using const_pointer = value_type const*;

private:
  struct no_value_t {};
  union slot {
    no_value_t no_value;
    value_type value;
    constexpr ~slot() {}
  };

  std::array<slot, Capacity> data_{};
  static constexpr size_type capacity_ = Capacity;
  size_type front_ = 0;
  size_type size_ = 0;

  constexpr size_type index_of(size_type offset) const {
    return (front_ + offset) % capacity_;
  }

  constexpr pointer slot_ptr(size_type i) {
    return &data_[i].value;
  }

  constexpr const_pointer slot_ptr(size_type i) const {
    return &data_[i].value;
  }

public:
  constexpr inplace_queue() = default;

  constexpr inplace_queue(inplace_queue const& other)
    requires std::is_copy_constructible_v<T> {
    for (size_type i = 0; i < other.size_; ++i) {
      std::construct_at(slot_ptr(i), *other.slot_ptr(other.index_of(i)));
      ++size_;
    }
  }

  constexpr inplace_queue(
      inplace_queue&& other
  ) noexcept(std::is_nothrow_move_constructible_v<T>)
    requires std::is_move_constructible_v<T> {
    for (size_type i = 0; i < other.size_; ++i) {
      std::construct_at(
          slot_ptr(i),
          std::move(*other.slot_ptr(other.index_of(i)))
      );
      ++size_;
    }
    other.clear();
  }

  constexpr inplace_queue& operator=(inplace_queue const& other)
    requires std::is_copy_constructible_v<T> {
    if (this != &other) {
      clear();
      for (size_type i = 0; i < other.size_; ++i) {
        std::construct_at(slot_ptr(i), *other.slot_ptr(other.index_of(i)));
        ++size_;
      }
    }
    return *this;
  }

  constexpr inplace_queue& operator=(
      inplace_queue&& other
  ) noexcept(std::is_nothrow_move_constructible_v<T>)
    requires std::is_move_constructible_v<T> {
    if (this != &other) {
      clear();
      for (size_type i = 0; i < other.size_; ++i) {
        std::construct_at(
            slot_ptr(i),
            std::move(*other.slot_ptr(other.index_of(i)))
        );
        ++size_;
      }
      other.clear();
    }
    return *this;
  }

  constexpr ~inplace_queue() {
    clear();
  }

  constexpr void clear() {
    for (size_type i = 0; i < size_; ++i) {
      std::destroy_at(slot_ptr(index_of(i)));
    }
    front_ = 0;
    size_ = 0;
  }

  [[nodiscard]] constexpr bool empty() const {
    return size_ == 0;
  }

  [[nodiscard]] constexpr bool full() const {
    return size_ == capacity_;
  }

  [[nodiscard]] constexpr size_type capacity() const {
    return capacity_;
  }

  [[nodiscard]] constexpr size_type size() const {
    return size_;
  }

  [[nodiscard]] constexpr const_reference front() const {
    assert(!empty());
    return *slot_ptr(front_);
  }

  [[nodiscard]] constexpr const_reference back() const {
    assert(!empty());
    return *slot_ptr(index_of(size_ - 1));
  }

  constexpr void push(value_type const& value)
    requires std::is_copy_constructible_v<value_type> {
    assert(!full());
    std::construct_at(slot_ptr(index_of(size_)), value);
    ++size_;
  }

  constexpr void push(value_type&& value)
    requires std::is_move_constructible_v<T> {
    assert(!full());
    std::construct_at(slot_ptr(index_of(size_)), std::move(value));
    ++size_;
  }

  template<typename... Args>
  constexpr void emplace(Args&&... args)
    requires std::is_constructible_v<T, Args...> {
    assert(!full());
    std::construct_at(slot_ptr(index_of(size_)), std::forward<Args>(args)...);
    ++size_;
  }

  [[nodiscard]] constexpr bool try_push(value_type const& value)
    requires std::is_copy_constructible_v<value_type> {
    if (full()) return false;
    push(value);
    return true;
  }

  [[nodiscard]] constexpr bool try_push(value_type&& value)
    requires std::is_move_constructible_v<T> {
    if (full()) return false;
    push(std::move(value));
    return true;
  }

  constexpr void pop() {
    assert(!empty());
    std::destroy_at(slot_ptr(front_));
    front_ = (front_ + 1) % capacity_;
    --size_;
  }

  [[nodiscard]] constexpr std::optional<value_type> try_pop()
    requires std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>
  {
    if (empty()) return std::nullopt;
    if constexpr (std::is_move_constructible_v<T>) {
      std::optional<value_type> result{std::move(*slot_ptr(front_))};
      pop();
      return result;
    } else {
      std::optional<value_type> result{*slot_ptr(front_)};
      pop();
      return result;
    }
  }
};

} // namespace emb
