#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace emb {

// Lock-free primitive for sharing data between ISR and thread context
// (or between two priority levels) without disabling interrupts.
// Uses signal fences — not suitable for multi-core (SMP) systems.
// Constraints:
//   - exactly one writer, multiple writers need separate mutual exclusion
//   - reader priority must not be higher than writer priority
template <typename T>
  requires(std::is_trivially_copyable_v<T>)
class isr_seqlock {
  using seq_type = std::atomic_unsigned_lock_free::value_type;
  seq_type volatile seq_ = 0;
  T value_{};
public:
  isr_seqlock() = default;
  isr_seqlock(isr_seqlock const&) = delete;
  isr_seqlock& operator=(isr_seqlock const&) = delete;

  void store(T const& desired) {
    seq_type s = seq_;
    seq_ = s + 1;
    std::atomic_signal_fence(std::memory_order::release);
    value_ = desired;
    std::atomic_signal_fence(std::memory_order::release);
    seq_ = s + 2;
  }

  template <typename F>
  void update(F&& f) {
    seq_type s = seq_;
    seq_ = s + 1;
    std::atomic_signal_fence(std::memory_order::release);
    value_ = std::forward<F>(f)(value_);
    std::atomic_signal_fence(std::memory_order::release);
    seq_ = s + 2;
  }

  T load() const {
    T snapshot;
    seq_type s1, s2;
    do {
      s1 = seq_;
      std::atomic_signal_fence(std::memory_order::acquire);
      snapshot = value_;
      std::atomic_signal_fence(std::memory_order::acquire);
      s2 = seq_;
    } while (s1 != s2 || (s1 & 1));
    return snapshot;
  }
};

}
