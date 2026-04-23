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
class circular_buffer {
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
  size_type front_ = 0;
  size_type size_ = 0;

public:
  constexpr circular_buffer() = default;

  constexpr circular_buffer(circular_buffer const&)
    requires std::is_trivially_copyable_v<T>
  = default;

  constexpr circular_buffer(circular_buffer const& other)
    requires(
        std::is_copy_constructible_v<T> && !std::is_trivially_copyable_v<T>
    ) {
    copy_construct_from(other);
  }

  constexpr circular_buffer(circular_buffer&&)
    requires std::is_trivially_copyable_v<T>
  = default;

  constexpr circular_buffer(
      circular_buffer&& other
  ) noexcept(std::is_nothrow_move_constructible_v<T>)
    requires(
        std::is_move_constructible_v<T> && !std::is_trivially_copyable_v<T>
    ) {
    move_construct_from(other);
  }

  constexpr circular_buffer& operator=(circular_buffer const&)
    requires std::is_trivially_copyable_v<T>
  = default;

  constexpr circular_buffer& operator=(circular_buffer const& other)
    requires(
        std::is_copy_constructible_v<T> && !std::is_trivially_copyable_v<T>
    ) {
    if (this != &other) {
      clear();
      copy_construct_from(other);
    }
    return *this;
  }

  constexpr circular_buffer& operator=(circular_buffer&&)
    requires std::is_trivially_copyable_v<T>
  = default;

  constexpr circular_buffer& operator=(
      circular_buffer&& other
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

  constexpr ~circular_buffer()
    requires std::is_trivially_destructible_v<T>
  = default;

  constexpr ~circular_buffer()
    requires(!std::is_trivially_destructible_v<T>) {
    clear();
  }

  constexpr void clear() {
    for (size_type i = 0; i < size_; ++i) {
      destroy_slot(index_of(i));
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

  [[nodiscard]] constexpr reference front() {
    ASSUME(!empty());
    return *slot_ptr(front_);
  }

  [[nodiscard]] constexpr const_reference front() const {
    ASSUME(!empty());
    return *slot_ptr(front_);
  }

  [[nodiscard]] constexpr reference back() {
    ASSUME(!empty());
    return *slot_ptr(index_of(size_ - 1));
  }

  [[nodiscard]] constexpr const_reference back() const {
    ASSUME(!empty());
    return *slot_ptr(index_of(size_ - 1));
  }

  [[nodiscard]] constexpr reference operator[](size_type i) {
    ASSUME(i < size_);
    return *slot_ptr(index_of(i));
  }

  [[nodiscard]] constexpr const_reference operator[](size_type i) const {
    ASSUME(i < size_);
    return *slot_ptr(index_of(i));
  }

  constexpr void push_back(value_type const& value)
    requires std::is_copy_constructible_v<T> {
    write_back_with([&](pointer p) { std::construct_at(p, value); });
  }

  constexpr void push_back(value_type&& value)
    requires std::is_move_constructible_v<T> {
    write_back_with([&](pointer p) { std::construct_at(p, std::move(value)); });
  }

  template<typename... Args>
  constexpr void emplace_back(Args&&... args)
    requires std::is_constructible_v<T, Args...> {
    write_back_with([&](pointer p) {
      std::construct_at(p, std::forward<Args>(args)...);
    });
  }

  constexpr void push_front(value_type const& value)
    requires std::is_copy_constructible_v<T> {
    write_front_with([&](pointer p) { std::construct_at(p, value); });
  }

  constexpr void push_front(value_type&& value)
    requires std::is_move_constructible_v<T> {
    write_front_with([&](pointer p) {
      std::construct_at(p, std::move(value));
    });
  }

  template<typename... Args>
  constexpr void emplace_front(Args&&... args)
    requires std::is_constructible_v<T, Args...> {
    write_front_with([&](pointer p) {
      std::construct_at(p, std::forward<Args>(args)...);
    });
  }

  [[nodiscard]] constexpr bool try_push_back(value_type const& value)
    requires std::is_copy_constructible_v<T> {
    if (full()) return false;
    push_back(value);
    return true;
  }

  [[nodiscard]] constexpr bool try_push_back(value_type&& value)
    requires std::is_move_constructible_v<T> {
    if (full()) return false;
    push_back(std::move(value));
    return true;
  }

  [[nodiscard]] constexpr bool try_push_front(value_type const& value)
    requires std::is_copy_constructible_v<T> {
    if (full()) return false;
    push_front(value);
    return true;
  }

  [[nodiscard]] constexpr bool try_push_front(value_type&& value)
    requires std::is_move_constructible_v<T> {
    if (full()) return false;
    push_front(std::move(value));
    return true;
  }

  constexpr void pop_front() {
    ASSUME(!empty());
    destroy_slot(front_);
    front_ = (front_ + 1) % capacity_;
    --size_;
  }

  constexpr void pop_back() {
    ASSUME(!empty());
    destroy_slot(index_of(size_ - 1));
    --size_;
  }

  [[nodiscard]] constexpr std::optional<value_type> try_pop_front()
    requires std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>
  {
    if (empty()) return std::nullopt;
    if constexpr (std::is_move_constructible_v<T>) {
      std::optional<value_type> result{std::move(*slot_ptr(front_))};
      pop_front();
      return result;
    } else {
      std::optional<value_type> result{*slot_ptr(front_)};
      pop_front();
      return result;
    }
  }

  [[nodiscard]] constexpr std::optional<value_type> try_pop_back()
    requires std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>
  {
    if (empty()) return std::nullopt;
    size_type const back_idx = index_of(size_ - 1);
    if constexpr (std::is_move_constructible_v<T>) {
      std::optional<value_type> result{std::move(*slot_ptr(back_idx))};
      pop_back();
      return result;
    } else {
      std::optional<value_type> result{*slot_ptr(back_idx)};
      pop_back();
      return result;
    }
  }

  constexpr void fill(value_type const& value)
    requires std::is_copy_constructible_v<T> {
    clear();
    for (size_type i = 0; i < capacity_; ++i) {
      std::construct_at(slot_ptr(i), value);
    }
    size_ = capacity_;
  }

private:
  constexpr size_type index_of(size_type offset) const {
    return (front_ + offset) % capacity_;
  }

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

  template<typename F>
  constexpr void write_back_with(F&& construct_fn) {
    size_type const slot_idx = index_of(size_);
    if (full()) {
      destroy_slot(slot_idx);
      construct_fn(slot_ptr(slot_idx));
      front_ = (front_ + 1) % capacity_;
    } else {
      construct_fn(slot_ptr(slot_idx));
      ++size_;
    }
  }

  template<typename F>
  constexpr void write_front_with(F&& construct_fn) {
    size_type const new_front = (front_ + capacity_ - 1) % capacity_;
    if (full()) {
      destroy_slot(new_front);
      construct_fn(slot_ptr(new_front));
    } else {
      construct_fn(slot_ptr(new_front));
      ++size_;
    }
    front_ = new_front;
  }

  constexpr void copy_construct_from(circular_buffer const& other) {
    for (size_type i = 0; i < other.size_; ++i) {
      std::construct_at(slot_ptr(i), *other.slot_ptr(other.index_of(i)));
      ++size_;
    }
  }

  constexpr void move_construct_from(
      circular_buffer& other
  ) noexcept(std::is_nothrow_move_constructible_v<T>) {
    for (size_type i = 0; i < other.size_; ++i) {
      std::construct_at(
          slot_ptr(i),
          std::move(*other.slot_ptr(other.index_of(i)))
      );
      ++size_;
    }
    other.clear();
  }
};

} // namespace emb
