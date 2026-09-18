#pragma once

#include <emb/concurrent/memory_scope.hpp>

#include <atomic>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace emb {

// A seqlock with two copies of T, so that a reader never waits for a
// writer it preempts (the latch of Linux's seqcount_latch).
//
// The writer makes seq_ odd and writes value_[0], then makes seq_ even
// and writes value_[1]. A reader copies value_[seq_ & 1], the copy the
// writer is not touching, and keeps it if seq_ did not change meanwhile.
// A reader that preempts the writer finds seq_ unchanged and never
// retries, so it may run at any priority, NMI included; only a reader the
// writer preempts, or one racing it on another core, retries. The price
// is twice the memory and two copies per store().
//
// store() and load() come in one version per scope, side by side, as in
// seqlock: memory_scope::local (local_latched_seqlock) with signal fences
// for the writer's own core, memory_scope::smp (the default) with thread
// fences and acquire/release operations for other cores too.
// Constraints:
//   - exactly one writer; several need their own mutual exclusion
//   - a reader the writer interrupts during every copy never finishes
template<typename T, memory_scope Scope = memory_scope::smp>
  requires(std::is_trivially_copyable_v<T>)
class latched_seqlock {
private:
  std::atomic<std::uint32_t> seq_ = 0;
  T value_[2]{};
public:
  latched_seqlock() = default;
  latched_seqlock(latched_seqlock const&) = delete;
  latched_seqlock& operator=(latched_seqlock const&) = delete;

  void store(T const& desired)
    requires(Scope == memory_scope::local)
  {
    std::uint32_t const s = seq_.load(std::memory_order::relaxed);
    std::atomic_signal_fence(std::memory_order::release);
    seq_.store(s + 1, std::memory_order::relaxed);
    std::atomic_signal_fence(std::memory_order::release);
    value_[0] = desired;
    std::atomic_signal_fence(std::memory_order::release);
    seq_.store(s + 2, std::memory_order::relaxed);
    std::atomic_signal_fence(std::memory_order::release);
    value_[1] = desired;
  }

  void store(T const& desired)
    requires(Scope == memory_scope::smp)
  {
    std::uint32_t const s = seq_.load(std::memory_order::relaxed);
    seq_.store(s + 1, std::memory_order::release);
    std::atomic_thread_fence(std::memory_order::release);
    value_[0] = desired;
    seq_.store(s + 2, std::memory_order::release);
    std::atomic_thread_fence(std::memory_order::release);
    value_[1] = desired;
  }

  // Calls f on the value stored last.
  template<typename F>
  void update(F&& f)
  {
    store(std::forward<F>(f)(std::as_const(value_[1])));
  }

  T load() const
    requires(Scope == memory_scope::local)
  {
    for (;;) {
      std::uint32_t const s = seq_.load(std::memory_order::relaxed);
      std::atomic_signal_fence(std::memory_order::acquire);
      T const snapshot = value_[s & 1];
      std::atomic_signal_fence(std::memory_order::acquire);
      if (seq_.load(std::memory_order::relaxed) == s) {
        return snapshot;
      }
    }
  }

  T load() const
    requires(Scope == memory_scope::smp)
  {
    for (;;) {
      std::uint32_t const s = seq_.load(std::memory_order::acquire);
      T const snapshot = value_[s & 1];
      std::atomic_thread_fence(std::memory_order::acquire);
      if (seq_.load(std::memory_order::relaxed) == s) {
        return snapshot;
      }
    }
  }
};

template<typename T>
using local_latched_seqlock = latched_seqlock<T, memory_scope::local>;

} // namespace emb
