#pragma once

#include <emb/concurrent/memory_scope.hpp>

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace emb {

// Wait-free buffer for one writer and several readers: load() returns the
// value stored last, whole, and neither side ever waits for the other.
//
// A triple buffer that takes more than one reader. latest_ carries the
// index of the slot holding the latest value in its low bits and, above
// them, how many loads have claimed that slot since it was stored. A load
// takes the index and counts itself in with one fetch_add, copies the slot
// out and gives it back by decrementing the slot's held_ counter. store()
// fills a slot that is neither latest nor held, exchanges its index into
// latest_ and adds the claims the exchange took to the old slot's counter.
// That counter returns to zero once the last of those loads is done with
// the slot; loads that finish before the writer adds their claims take it
// below zero until it does. Both counters step by one claim, the bit above
// the index, so they wrap in step and a free slot reads exactly zero.
//
// With at most Readers loads holding a slot, Readers + 2 slots always leave
// store() one to fill. Unlike latched_seqlock, a load never retries,
// whichever of it and store() preempts the other and whichever core it runs
// on; unlike a triple buffer per reader, store() copies the value once. The
// price is Readers + 2 copies of T, a counter per slot and two atomic
// read-modify-writes per load(). On ARMv7-M those are ldrex/strex loops,
// which repeat only when an interrupt lands between the two, as
// triple_buffer's exchange does.
//
// store() and load() come in one version per scope, side by side, as in
// seqlock: memory_scope::local (local_spmc_buffer) with relaxed operations
// and signal fences for the writer's own core, memory_scope::smp (the
// default) with acquire/release operations and a thread fence for other
// cores too.
// Constraints:
//   - exactly one writer; several need their own mutual exclusion
//   - at most Readers loads in progress while store() runs. Counting every
//     context that calls load() is always enough. On one core only the
//     contexts the writer can preempt need counting: a load from any other
//     one runs to completion while store() is suspended, or outside it
//     altogether. With more loads in progress, store() spins until one
//     ends, which on one core never happens if store() preempted it.
template<typename T,
         std::size_t Readers,
         memory_scope Scope = memory_scope::smp>
  requires(std::is_trivially_copyable_v<T>
           && std::is_default_constructible_v<T>)
class spmc_buffer {
private:
  static constexpr std::size_t slot_count = Readers + 2;
  static constexpr std::uint32_t index_mask =
      std::uint32_t(std::bit_ceil(slot_count) - 1);
  static constexpr std::uint32_t claim = index_mask + 1;

  static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
                "spmc_buffer requires hardware atomics");

  T buf_[slot_count]{};
  mutable std::atomic<std::uint32_t> latest_{0};
  mutable std::atomic<std::uint32_t> held_[slot_count]{};
public:
  spmc_buffer() = default;
  spmc_buffer(spmc_buffer const&) = delete;
  spmc_buffer& operator=(spmc_buffer const&) = delete;

  void store(T const& value)
    requires(Scope == memory_scope::local)
  {
    std::uint32_t const next = vacant();
    std::atomic_signal_fence(std::memory_order::acquire);
    buf_[next] = value;
    std::atomic_signal_fence(std::memory_order::release);
    std::uint32_t const prev =
        latest_.exchange(next, std::memory_order::relaxed);
    held_[prev & index_mask].fetch_add(prev & ~index_mask,
                                       std::memory_order::relaxed);
  }

  void store(T const& value)
    requires(Scope == memory_scope::smp)
  {
    std::uint32_t const next = vacant();
    std::atomic_thread_fence(std::memory_order::acquire);
    buf_[next] = value;
    std::uint32_t const prev =
        latest_.exchange(next, std::memory_order::release);
    held_[prev & index_mask].fetch_add(prev & ~index_mask,
                                       std::memory_order::relaxed);
  }

  T load() const
    requires(Scope == memory_scope::local)
  {
    std::uint32_t const i =
        latest_.fetch_add(claim, std::memory_order::relaxed) & index_mask;
    std::atomic_signal_fence(std::memory_order::acquire);
    T const value = buf_[i];
    std::atomic_signal_fence(std::memory_order::release);
    held_[i].fetch_sub(claim, std::memory_order::relaxed);
    return value;
  }

  T load() const
    requires(Scope == memory_scope::smp)
  {
    std::uint32_t const i =
        latest_.fetch_add(claim, std::memory_order::acquire) & index_mask;
    T const value = buf_[i];
    held_[i].fetch_sub(claim, std::memory_order::release);
    return value;
  }
private:
  // A slot that is neither latest nor held. Readers + 1 slots are not latest
  // and at most Readers of them are held, so one is free when the scan
  // starts, and it stays free: while store() looks, loads can only give
  // slots back, since the ones they claim are latest. In the local scope
  // the first pass finds it. In the smp scope a relaxed load may still show
  // a count that a reader on another core has already given back, so a
  // pass can miss the free slot until that decrement reaches this core;
  // that is the only reason the loop goes round again. The fence after the
  // call is what makes a zero count mean that the loads which held the
  // slot are done reading it.
  std::uint32_t vacant() const
  {
    std::uint32_t const latest =
        latest_.load(std::memory_order::relaxed) & index_mask;
    for (std::uint32_t i = 0;; i = (i + 1 == slot_count) ? 0 : i + 1) {
      if (i != latest && held_[i].load(std::memory_order::relaxed) == 0) {
        return i;
      }
    }
  }
};

template<typename T, std::size_t Readers>
using local_spmc_buffer = spmc_buffer<T, Readers, memory_scope::local>;

} // namespace emb
