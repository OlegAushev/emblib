#pragma once

#include <atomic>
#include <cstdint>

namespace emb {

/// Wait-free triple buffer for single-writer / single-reader.
///
/// No constraint on writer commit rate — the reader always gets
/// the latest committed value without torn reads.
/// Requires hardware atomics (LDREX/STREX on Cortex-M).
template <typename T>
  requires(std::is_trivially_copyable_v<T>)
class triple_buffer {
public:
  void store(T const& value) {
    buf_[write_] = value;
    std::atomic_signal_fence(std::memory_order_release);
    write_ = shared_.exchange(write_, std::memory_order_relaxed);
  }

  T load() const {
    read_ = shared_.exchange(read_, std::memory_order_relaxed);
    std::atomic_signal_fence(std::memory_order_acquire);
    return buf_[read_];
  }

private:
  T buf_[3]{};
  std::atomic<uint8_t> mutable shared_{0};
  uint8_t write_ = 1;
  uint8_t mutable read_ = 2;
};

} // namespace emb
