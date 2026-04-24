#pragma once

#include <emb/assert.hpp>

#include <array>
#include <atomic>
#include <bit>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace emb {

// Lock-free single-producer / single-consumer queue for ISR context.
//
// Concurrency contract:
//   - Producer side (one thread/ISR only): try_push, try_emplace.
//   - Consumer side (one thread/ISR only): try_pop, front, clear.
//   - Producer and consumer may run concurrently; two producers or
//     two consumers may not.
//
// Capacity must be a power of two so indexing uses a mask instead of %.
// Storage is in-place via a union slot, so T is never default-constructed;
// only pushed elements are alive.
template<typename T, std::size_t Capacity>
  requires(std::has_single_bit(Capacity))
class isr_spsc_inplace_queue {
public:
  using value_type = T;
  using atomic_index_type = std::atomic_unsigned_lock_free;
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
  static constexpr size_type mask_ = Capacity - 1;
  atomic_index_type front_ = 0;
  atomic_index_type back_ = 0;

public:
  isr_spsc_inplace_queue() = default;
  isr_spsc_inplace_queue(isr_spsc_inplace_queue const&) = delete;
  isr_spsc_inplace_queue(isr_spsc_inplace_queue&&) = delete;
  isr_spsc_inplace_queue& operator=(isr_spsc_inplace_queue const&) = delete;
  isr_spsc_inplace_queue& operator=(isr_spsc_inplace_queue&&) = delete;

  ~isr_spsc_inplace_queue()
    requires(std::is_trivially_destructible_v<T>)
  = default;

  ~isr_spsc_inplace_queue()
    requires(!std::is_trivially_destructible_v<T>) {
    clear();
  }

  // Consumer-side. Destroys all live elements and resets to empty.
  // Must not run concurrently with try_pop/front on the consumer side.
  void clear() {
    auto const b = back_.load(std::memory_order::acquire);
    auto f = front_.load(std::memory_order::relaxed);
    while (f != b) {
      destroy_slot(index_of(f));
      ++f;
    }
    front_.store(b, std::memory_order::release);
  }

  // Observers. Return an advisory snapshot: the state may change before
  // the caller acts on it. Use try_push/try_pop for precise checks.
  [[nodiscard]] bool empty() const {
    return front_.load(std::memory_order::acquire)
        == back_.load(std::memory_order::acquire);
  }

  [[nodiscard]] bool full() const {
    return size() == capacity_;
  }

  [[nodiscard]] size_type capacity() const {
    return capacity_;
  }

  [[nodiscard]] size_type size() const {
    auto const f = front_.load(std::memory_order::acquire);
    auto const b = back_.load(std::memory_order::acquire);
    return size_type(b - f);
  }

  // Producer-side. Returns false if the queue is full.
  [[nodiscard]] bool try_push(value_type const& value)
    requires std::is_copy_constructible_v<T> {
    return try_emplace(value);
  }

  [[nodiscard]] bool try_push(value_type&& value)
    requires std::is_move_constructible_v<T> {
    return try_emplace(std::move(value));
  }

  template<typename... Args>
  [[nodiscard]] bool try_emplace(Args&&... args)
    requires std::is_constructible_v<T, Args...> {
    auto const b = back_.load(std::memory_order::relaxed);
    if (b - front_.load(std::memory_order::acquire) == capacity_) return false;
    std::construct_at(slot_ptr(index_of(b)), std::forward<Args>(args)...);
    back_.store(b + 1, std::memory_order::release);
    return true;
  }

  // Consumer-side. Non-destructive peek at the head; returns a copy.
  [[nodiscard]] std::optional<value_type> front() const
    requires std::is_copy_constructible_v<T> {
    auto const f = front_.load(std::memory_order::relaxed);
    if (f == back_.load(std::memory_order::acquire)) return std::nullopt;
    return std::optional<value_type>(*slot_ptr(index_of(f)));
  }

  // Consumer-side. Removes and returns the head element, or nullopt if empty.
  [[nodiscard]] std::optional<value_type> try_pop()
    requires std::is_move_constructible_v<T> {
    auto const f = front_.load(std::memory_order::relaxed);
    if (f == back_.load(std::memory_order::acquire)) return std::nullopt;
    auto const f_idx = index_of(f);
    std::optional<value_type> result{std::move(*slot_ptr(f_idx))};
    destroy_slot(f_idx);
    front_.store(f + 1, std::memory_order::release);
    return result;
  }

private:
  constexpr size_type index_of(atomic_index_type::value_type abs_idx) const {
    return size_type(abs_idx) & mask_;
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
};

} // namespace emb
