// Simplified constexpr version of std::lock_guard.

#pragma once

#ifdef __cpp_concepts

#include <emb/core.hpp>

namespace emb {

template<typename Mutex>
  requires requires(Mutex& m) {
    { m.lock() };
    { m.unlock() };
  }
class lock_guard {
public:
  using mutex_type = Mutex;

  [[nodiscard]] constexpr explicit lock_guard(mutex_type& m) : mutex_(m) {
    mutex_.lock();
  }

  constexpr ~lock_guard() {
    mutex_.unlock();
  }

  lock_guard(lock_guard const&) = delete;
  lock_guard& operator=(lock_guard const&) = delete;
private:
  mutex_type& mutex_;
};

} // namespace emb

#endif
