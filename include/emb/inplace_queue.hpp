#pragma once

#include <emb/core.hpp>

#include <array>
#include <optional>
#include <type_traits>

namespace emb {

template<typename T, size_t Capacity>
  requires std::is_default_constructible_v<T>
        && std::is_trivially_destructible_v<T>
        && (Capacity > 0)
class inplace_queue {
public:
  using value_type = T;
  using size_type = size_t;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = value_type*;
  using const_pointer = value_type const*;
  using underlying_type = std::array<value_type, Capacity>;
private:
  underlying_type data_{};
  static constexpr size_type capacity_ = Capacity;
  size_type front_ = 0;
  size_type size_ = 0;
public:
  constexpr inplace_queue() = default;

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

  constexpr void push(value_type const& value) {
    assert(!full());
    data_[(front_ + size_) % capacity_] = value;
    ++size_;
  }

  [[nodiscard]] constexpr bool try_push(value_type const& value) {
    if (full()) return false;
    push(value);
    return true;
  }

  [[nodiscard]] constexpr const_reference front() const {
    assert(!empty());
    return data_[front_];
  }

  [[nodiscard]] constexpr const_reference back() const {
    assert(!empty());
    return data_[(front_ + size_ - 1) % capacity_];
  }

  constexpr void pop() {
    assert(!empty());
    front_ = (front_ + 1) % capacity_;
    --size_;
  }

  [[nodiscard]] constexpr std::optional<value_type> try_pop() {
    if (empty()) return std::nullopt;
    auto v = front();
    pop();
    return v;
  }
};

} // namespace emb
