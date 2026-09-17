#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace emb {

// Wait-free triple buffer for single-writer / single-reader.
//
// No constraint on the commit rate: store() never waits for the reader, and
// load() returns the latest committed value without torn reads. Repeated
// load() calls with no commit in between keep returning that same value.
//
// Both exchanges are acq_rel, because buffers travel in both directions.
// The release half publishes the value just written; the acquire half claims
// the buffer coming back, which the other side has only just stopped using —
// the reader hands back what it read last time, the writer what it wrote.
// Release alone would let the writer's next stores drift ahead of the swap,
// and acquire alone would let the reader's last loads drift past it, either
// way straight into the buffer the other side has already taken.
//
// Exactly one context may call store() and exactly one may call load().
// Both indices are plain bytes and the check-then-exchange in load() is not
// atomic as a pair, so two contexts sharing either side race on them. A
// second reader is the easy mistake to make: it hands a live buffer back to
// the writer and can leave the real reader pinned to a stale one.
template<typename T>
  requires(std::is_trivially_copyable_v<T>
           && std::is_default_constructible_v<T>)
class triple_buffer {
private:
  static constexpr std::uint8_t index_mask = 0b011;
  static constexpr std::uint8_t fresh_bit = 0b100;

  static_assert(std::atomic<std::uint8_t>::is_always_lock_free,
                "triple_buffer requires hardware atomics");

  T buf_[3]{};
  std::atomic<std::uint8_t> shared_{0};
  std::uint8_t write_ = 1;
  std::uint8_t read_ = 2;
public:
  void store(T const& value)
  {
    buf_[write_] = value;
    write_ = shared_.exchange(std::uint8_t(write_ | fresh_bit),
                              std::memory_order_acq_rel)
           & index_mask;
  }

  T load()
  {
    if (shared_.load(std::memory_order_relaxed) & fresh_bit) {
      read_ = shared_.exchange(read_, std::memory_order_acq_rel) & index_mask;
    }
    return buf_[read_];
  }
};

} // namespace emb
