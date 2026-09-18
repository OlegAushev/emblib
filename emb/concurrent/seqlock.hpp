#pragma once

#include <emb/concurrent/memory_scope.hpp>

#include <atomic>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace emb {

// Sequence lock: one writer publishes a T, readers copy it out and retry
// while a write is in progress.
//
// The writer makes seq_ odd, writes value_ and makes seq_ even again; a
// reader keeps its copy only if seq_ was even and unchanged around it.
// Readers never hold the writer up, but a reader that preempts the writer
// mid-write spins until the writer resumes, which on one core it never
// does.
//
// store() and load() come in one version per scope, side by side.
// memory_scope::local (local_seqlock) serves the writer's own core only:
// signal fences, which restrain just the compiler. memory_scope::smp (the
// default) serves other cores too: thread fences and acquire/release
// operations, which the hardware has to honour as well.
// Constraints:
//   - exactly one writer; several need their own mutual exclusion
//   - no reader may preempt the writer
template<typename T, memory_scope Scope = memory_scope::smp>
  requires(std::is_trivially_copyable_v<T>)
class seqlock {
private:
  std::atomic<std::uint32_t> seq_ = 0;
  T value_{};
public:
  seqlock() = default;
  seqlock(seqlock const&) = delete;
  seqlock& operator=(seqlock const&) = delete;

  void store(T const& desired)
    requires(Scope == memory_scope::local)
  {
    std::uint32_t const s = seq_.load(std::memory_order::relaxed);
    seq_.store(s + 1, std::memory_order::relaxed);
    std::atomic_signal_fence(std::memory_order::release);
    value_ = desired;
    std::atomic_signal_fence(std::memory_order::release);
    seq_.store(s + 2, std::memory_order::relaxed);
  }

  void store(T const& desired)
    requires(Scope == memory_scope::smp)
  {
    std::uint32_t const s = seq_.load(std::memory_order::relaxed);
    seq_.store(s + 1, std::memory_order::relaxed);
    std::atomic_thread_fence(std::memory_order::release);
    value_ = desired;
    seq_.store(s + 2, std::memory_order::release);
  }

  // Calls f outside the write, on the value stored last.
  template<typename F>
  void update(F&& f)
  {
    store(std::forward<F>(f)(std::as_const(value_)));
  }

  T load() const
    requires(Scope == memory_scope::local)
  {
    for (;;) {
      std::uint32_t const s = seq_.load(std::memory_order::relaxed);
      std::atomic_signal_fence(std::memory_order::acquire);
      if (s & 1) {
        continue;
      }
      T const snapshot = value_;
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
      if (s & 1) {
        continue;
      }
      T const snapshot = value_;
      std::atomic_thread_fence(std::memory_order::acquire);
      if (seq_.load(std::memory_order::relaxed) == s) {
        return snapshot;
      }
    }
  }
};

template<typename T>
using local_seqlock = seqlock<T, memory_scope::local>;

} // namespace emb
