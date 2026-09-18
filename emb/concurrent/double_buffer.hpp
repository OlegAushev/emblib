#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace emb {

// Wait-free double buffer for single-writer / multi-reader.
// Uses signal fences — not suitable for multi-core (SMP) systems.
//
// The writer alternates slots, so the slot a reader is copying is the one
// the writer fills next. A reader the writer can preempt gets a torn value
// if the writer commits twice during one load(), and nothing detects it.
// Hence the constraint: the writer must not commit more than once during
// a single load(). It holds by construction when the writer cannot preempt
// the reader: reader in an ISR, writer in main or in a lower-priority ISR.
// For the other direction, writer in an ISR and reader in main, use
// triple_buffer or local_seqlock instead.
template<typename T>
  requires(std::is_trivially_copyable_v<T>)
class double_buffer {
private:
  T buf_[2]{};
  std::atomic<std::uint8_t> front_{0};
public:
  void store(T const& value)
  {
    std::uint8_t back = 1 - front_.load(std::memory_order_relaxed);
    buf_[back] = value;
    std::atomic_signal_fence(std::memory_order_release);
    front_.store(back, std::memory_order_relaxed);
  }

  T load() const
  {
    std::uint8_t front = front_.load(std::memory_order_relaxed);
    std::atomic_signal_fence(std::memory_order_acquire);
    return buf_[front];
  }
};

} // namespace emb
