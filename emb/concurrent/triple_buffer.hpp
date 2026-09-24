#pragma once

#include <emb/concurrent/memory_scope.hpp>

#include <atomic>
#include <type_traits>

namespace emb {

// Wait-free triple buffer for single-writer / single-reader.
//
// No constraint on the commit rate: store() never waits for the reader, and
// load() returns the latest committed value without torn reads. Repeated
// load() calls with no commit in between keep returning that same value.
//
// Both exchanges act as acq_rel, because buffers travel in both directions.
// The release half publishes the value just written; the acquire half claims
// the buffer coming back, which the other side has only just stopped using —
// the reader hands back what it read last time, the writer what it wrote.
// Release alone would let the writer's next stores drift ahead of the swap,
// and acquire alone would let the reader's last loads drift past it, either
// way straight into the buffer the other side has already taken.
//
// store() and load() come in one version per scope, side by side, as in
// seqlock. memory_scope::local (local_triple_buffer) serves the writer's own
// core only: a relaxed exchange between a release and an acquire signal
// fence, which restrain just the compiler. memory_scope::smp (the default)
// serves other cores too: an acq_rel exchange, which the hardware has to
// honour as well.
//
// Exactly one context may call store() and exactly one may call load().
// Both indices are plain members and the check-then-exchange in load() is
// not atomic as a pair, so two contexts sharing either side race on them. A
// second reader is the easy mistake to make: it hands a live buffer back to
// the writer and can leave the real reader pinned to a stale one.
template<typename T, memory_scope Scope = memory_scope::smp>
  requires(std::is_trivially_copyable_v<T>
           && std::is_default_constructible_v<T>)
class triple_buffer {
private:
  using index_type = std::atomic_unsigned_lock_free::value_type;

  static constexpr index_type index_mask = 0b011;
  static constexpr index_type fresh_bit = 0b100;

  T buf_[3]{};
  std::atomic_unsigned_lock_free shared_{0};
  index_type write_ = 1;
  index_type read_ = 2;
public:
  void store(T const& value)
    requires(Scope == memory_scope::local)
  {
    buf_[write_] = value;
    std::atomic_signal_fence(std::memory_order_release);
    write_ = shared_.exchange(write_ | fresh_bit, std::memory_order_relaxed)
           & index_mask;
    std::atomic_signal_fence(std::memory_order_acquire);
  }

  void store(T const& value)
    requires(Scope == memory_scope::smp)
  {
    buf_[write_] = value;
    write_ = shared_.exchange(write_ | fresh_bit, std::memory_order_acq_rel)
           & index_mask;
  }

  T load()
    requires(Scope == memory_scope::local)
  {
    if (shared_.load(std::memory_order_relaxed) & fresh_bit) {
      std::atomic_signal_fence(std::memory_order_release);
      read_ = shared_.exchange(read_, std::memory_order_relaxed) & index_mask;
      std::atomic_signal_fence(std::memory_order_acquire);
    }
    return buf_[read_];
  }

  T load()
    requires(Scope == memory_scope::smp)
  {
    if (shared_.load(std::memory_order_relaxed) & fresh_bit) {
      read_ = shared_.exchange(read_, std::memory_order_acq_rel) & index_mask;
    }
    return buf_[read_];
  }
};

template<typename T>
using local_triple_buffer = triple_buffer<T, memory_scope::local>;

} // namespace emb
