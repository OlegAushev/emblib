#pragma once

#include <emb/assert.hpp>

#include <array>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace emb {

template<typename T, std::size_t Capacity>
  requires(Capacity > 0)
class inplace_stack {
public:
  using value_type = T;
  using size_type = std::size_t;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = value_type*;
  using const_pointer = value_type const*;

private:
  struct no_value_t {};
  union slot {
    no_value_t no_value;
    value_type value;
    constexpr slot() : no_value{} {}
    constexpr slot(slot const&) = default;
    constexpr slot(slot&&) = default;
    constexpr slot& operator=(slot const&) = default;
    constexpr slot& operator=(slot&&) = default;
    constexpr ~slot()
      requires std::is_trivially_destructible_v<T>
    = default;
    constexpr ~slot()
      requires(!std::is_trivially_destructible_v<T>) {}
  };

  std::array<slot, Capacity> data_{};
  static constexpr size_type capacity_ = Capacity;
  size_type size_ = 0;

public:
  constexpr inplace_stack() = default;

  constexpr inplace_stack(inplace_stack const&)
    requires std::is_trivially_copyable_v<T>
  = default;

  constexpr inplace_stack(inplace_stack const& other)
    requires(
        std::is_copy_constructible_v<T> && !std::is_trivially_copyable_v<T>
    ) {
    copy_construct_from(other);
  }

  constexpr inplace_stack(inplace_stack&&)
    requires std::is_trivially_copyable_v<T>
  = default;

  constexpr inplace_stack(
      inplace_stack&& other
  ) noexcept(std::is_nothrow_move_constructible_v<T>)
    requires(
        std::is_move_constructible_v<T> && !std::is_trivially_copyable_v<T>
    ) {
    move_construct_from(other);
  }

  constexpr inplace_stack& operator=(inplace_stack const&)
    requires std::is_trivially_copyable_v<T>
  = default;

  constexpr inplace_stack& operator=(inplace_stack const& other)
    requires(
        std::is_copy_constructible_v<T> && !std::is_trivially_copyable_v<T>
    ) {
    if (this != &other) {
      clear();
      copy_construct_from(other);
    }
    return *this;
  }

  constexpr inplace_stack& operator=(inplace_stack&&)
    requires std::is_trivially_copyable_v<T>
  = default;

  constexpr inplace_stack& operator=(
      inplace_stack&& other
  ) noexcept(std::is_nothrow_move_constructible_v<T>)
    requires(
        std::is_move_constructible_v<T> && !std::is_trivially_copyable_v<T>
    ) {
    if (this != &other) {
      clear();
      move_construct_from(other);
    }
    return *this;
  }

  constexpr ~inplace_stack()
    requires std::is_trivially_destructible_v<T>
  = default;

  constexpr ~inplace_stack()
    requires(!std::is_trivially_destructible_v<T>) {
    clear();
  }

  constexpr void clear() {
    for (size_type i = size_; i > 0; --i) {
      destroy_slot(i - 1);
    }
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

  [[nodiscard]] constexpr reference top() {
    ASSUME(!empty());
    return *slot_ptr(size_ - 1);
  }

  [[nodiscard]] constexpr const_reference top() const {
    ASSUME(!empty());
    return *slot_ptr(size_ - 1);
  }

  constexpr void push(value_type const& value)
    requires std::is_copy_constructible_v<value_type> {
    ASSUME(!full());
    std::construct_at(slot_ptr(size_), value);
    ++size_;
  }

  constexpr void push(value_type&& value)
    requires std::is_move_constructible_v<T> {
    ASSUME(!full());
    std::construct_at(slot_ptr(size_), std::move(value));
    ++size_;
  }

  template<typename... Args>
  constexpr void emplace(Args&&... args)
    requires std::is_constructible_v<T, Args...> {
    ASSUME(!full());
    std::construct_at(slot_ptr(size_), std::forward<Args>(args)...);
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
    ASSUME(!empty());
    destroy_slot(size_ - 1);
    --size_;
  }

  [[nodiscard]] constexpr std::optional<value_type> try_pop()
    requires std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>
  {
    if (empty()) return std::nullopt;
    if constexpr (std::is_move_constructible_v<T>) {
      std::optional<value_type> result{std::move(*slot_ptr(size_ - 1))};
      pop();
      return result;
    } else {
      std::optional<value_type> result{*slot_ptr(size_ - 1)};
      pop();
      return result;
    }
  }

private:
  constexpr pointer slot_ptr(size_type i) {
    return &data_[i].value;
  }

  constexpr const_pointer slot_ptr(size_type i) const {
    return &data_[i].value;
  }

  constexpr void destroy_slot(size_type i) {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      std::destroy_at(slot_ptr(i));
    }
  }

  constexpr void copy_construct_from(inplace_stack const& other) {
    for (size_type i = 0; i < other.size_; ++i) {
      std::construct_at(slot_ptr(i), *other.slot_ptr(i));
      ++size_;
    }
  }

  constexpr void move_construct_from(
      inplace_stack& other
  ) noexcept(std::is_nothrow_move_constructible_v<T>) {
    for (size_type i = 0; i < other.size_; ++i) {
      std::construct_at(slot_ptr(i), std::move(*other.slot_ptr(i)));
      ++size_;
    }
    other.clear();
  }
};

} // namespace emb
