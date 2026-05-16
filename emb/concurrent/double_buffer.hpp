#pragma once

#include <atomic>
#include <cstdint>

namespace emb {

// Wait-free double buffer for single-writer / multi-reader
//
// Invariant: writer must not commit more than once during a single load().
template <typename T>
  requires(std::is_trivially_copyable_v<T>)
class double_buffer {
public:
  void store(T const& value) {
    uint8_t back = 1 - front_;
    buf_[back] = value;
    std::atomic_signal_fence(std::memory_order_release);
    front_ = back;
  }

  T load() const {
    uint8_t front = front_;
    std::atomic_signal_fence(std::memory_order_acquire);
    return buf_[front];
  }

private:
  T buf_[2]{};
  uint8_t volatile front_ = 0;
};

} // namespace emb
