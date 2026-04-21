#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

namespace emb {

// Lock-free primitive for sharing data between threads without blocking.
// Uses atomic operations with release/acquire ordering —
// safe on multi-core (SMP) systems.
// Constraints:
//   - exactly one writer, multiple writers need separate mutual exclusion
//   - readers retry while the writer is active (not wait-free)
template <typename T>
  requires(std::is_trivially_copyable_v<T>)
class seqlock {
  std::atomic<uint32_t> seq_ = 0;
  T value_{};
public:
  seqlock() = default;
  seqlock(seqlock const&) = delete;
  seqlock& operator=(seqlock const&) = delete;

  void store(T const& desired) {
    uint32_t const s = seq_.load(std::memory_order::relaxed);
    seq_.store(s + 1, std::memory_order::relaxed);
    std::atomic_thread_fence(std::memory_order::release);
    std::memcpy(&value_, &desired, sizeof(T));
    seq_.store(s + 2, std::memory_order::release);
  }

  template<typename F>
  void update(F&& f) {
    uint32_t const s = seq_.load(std::memory_order::relaxed);
    seq_.store(s + 1, std::memory_order::relaxed);
    std::atomic_thread_fence(std::memory_order::release);
    T snapshot;
    std::memcpy(&snapshot, &value_, sizeof(T));
    T desired = std::forward<F>(f)(snapshot);
    std::memcpy(&value_, &desired, sizeof(T));
    seq_.store(s + 2, std::memory_order::release);
  }

  T load() const {
    T snapshot;
    uint32_t s1, s2;
    for(;;) {
      s1 = seq_.load(std::memory_order::acquire);
      if (s1 & 1) {
        continue;
      }
      std::memcpy(&snapshot, &value_, sizeof(T));
      std::atomic_thread_fence(std::memory_order::acquire);
      s2 = seq_.load(std::memory_order::relaxed);
      if (s1 == s2) return snapshot;
    }
  }
};

}
