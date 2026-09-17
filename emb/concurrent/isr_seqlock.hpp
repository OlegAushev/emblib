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
//   - a reader must not preempt the writer mid-write: ensure by priorities
//     or by masking interrupts around store()/update()
template<typename T>
  requires(std::is_trivially_copyable_v<T>)
class isr_seqlock {
private:
  std::atomic<std::uint32_t> seq_ = 0;
  T value_{};
public:
  isr_seqlock() = default;
  isr_seqlock(isr_seqlock const&) = delete;
  isr_seqlock& operator=(isr_seqlock const&) = delete;

  void store(T const& desired)
  {
    std::uint32_t const s = seq_.load(std::memory_order::relaxed);
    seq_.store(s + 1, std::memory_order::relaxed);
    std::atomic_signal_fence(std::memory_order::release);
    value_ = desired;
    std::atomic_signal_fence(std::memory_order::release);
    seq_.store(s + 2, std::memory_order::relaxed);
  }

  template<typename F>
  void update(F&& f)
  {
    std::uint32_t const s = seq_.load(std::memory_order::relaxed);
    seq_.store(s + 1, std::memory_order::relaxed);
    std::atomic_signal_fence(std::memory_order::release);
    value_ = std::forward<F>(f)(value_);
    std::atomic_signal_fence(std::memory_order::release);
    seq_.store(s + 2, std::memory_order::relaxed);
  }

  T load() const
  {
    T snapshot;
    std::uint32_t s1, s2;
    do {
      s1 = seq_.load(std::memory_order::relaxed);
      std::atomic_signal_fence(std::memory_order::acquire);
      snapshot = value_;
      std::atomic_signal_fence(std::memory_order::acquire);
      s2 = seq_.load(std::memory_order::relaxed);
    } while (s1 != s2 || (s1 & 1));
    return snapshot;
  }
};

} // namespace emb
