#pragma once

#include <atomic>
#include <type_traits>

namespace emb {

// The class template `double_buffer` is a wait-free buffer through which one
// context, the writer, publishes values of type `T` to any number of readers
// on the writer's core. `store()` commits a value, and `load()` returns a
// copy of the latest committed value, or `T{}` before the first commit.
//
// The writer fills the two slots in turn, so the slot a reader copies from
// survives the next commit and is overwritten by the `store()` after that. A
// reader that the writer cannot preempt, e.g. one in an interrupt handler
// that outranks the writer, therefore always gets the latest committed value,
// never a torn one. A reader that the writer outranks can get a torn value,
// and nothing detects it, if the writer commits more than once between the
// reader's call to `load()` and the reader's last use of the result. The
// window lasts past the return from `load()`: the compiler may assume that
// the slot does not change, since a change would be a data race, and may
// defer or repeat reads of the slot up to that last use. `local_triple_buffer`
// and `local_seqlock` have no such limit and suit a writer in an interrupt
// handler with a reader in the main loop.
//
// `double_buffer` uses signal fences, which restrain only the compiler.
// Unlike `triple_buffer` and `seqlock`, it has no version for several cores,
// where no priority keeps the writer from committing while a reader reads.
template<typename T>
  requires(std::is_trivially_copyable_v<T>
           && std::is_default_constructible_v<T>)
class double_buffer {
private:
  T buf_[2]{};
  std::atomic_unsigned_lock_free front_{0};
public:
  void store(T const& value)
  {
    auto const back = 1 - front_.load(std::memory_order_relaxed);
    buf_[back] = value;
    std::atomic_signal_fence(std::memory_order_release);
    front_.store(back, std::memory_order_relaxed);
  }

  T load() const
  {
    auto const front = front_.load(std::memory_order_relaxed);
    std::atomic_signal_fence(std::memory_order_acquire);
    return buf_[front];
  }
};

} // namespace emb
