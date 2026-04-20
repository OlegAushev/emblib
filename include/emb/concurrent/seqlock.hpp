#pragma once

#include <atomic>
#include <cstdint>

namespace emb {

// Lock-free primitive for sharing data between ISR and thread context
// (or between two priority levels) without disabling interrupts.
// Uses signal fences — not suitable for multi-core (SMP) systems.
// Constraints:
//   - exactly one writer, multiple writers need separate mutual exclusion
//   - reader priority must not be higher than writer priority
//   - protected data must be safe to read in a torn/inconsistent state
template <typename T>
  requires(std::is_trivially_copyable_v<T>)
class seqlock {
public:
  void store(T const& value) {
    uint32_t s = seq_;
    seq_ = s + 1;
    std::atomic_signal_fence(std::memory_order_release);
    value_ = value;
    std::atomic_signal_fence(std::memory_order_release);
    seq_ = s + 2;
  }

  template <typename F>
  void update(F&& f) {
    uint32_t s = seq_;
    seq_ = s + 1;
    std::atomic_signal_fence(std::memory_order_release);
    value_ = f(value_);
    std::atomic_signal_fence(std::memory_order_release);
    seq_ = s + 2;
  }

  T load() const {
    T result;
    uint32_t s1, s2;
    do {
      s1 = seq_;
      std::atomic_signal_fence(std::memory_order_acquire);
      result = value_;
      std::atomic_signal_fence(std::memory_order_acquire);
      s2 = seq_;
    } while (s1 != s2 || (s1 & 1));
    return result;
  }

private:
  uint32_t volatile seq_ = 0;
  T value_{};
};

}
