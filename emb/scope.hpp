#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

namespace emb {

// RAII wrapper that invokes a callback when the enclosing scope is exited,
// modeled after std::experimental::scope_exit (LFTS v3):
//
//   auto relock = emb::scope_exit([] { lock(); });
//
// The callback fires on every exit path, including early returns (e.g. from
// TRY). Call release() to dismiss it.
template<typename Callback>
  requires std::invocable<Callback&>
class [[nodiscard]] scope_exit {
public:
  template<typename Fn>
    requires(!std::same_as<std::remove_cvref_t<Fn>, scope_exit>)
  explicit scope_exit(Fn&& callback) : callback_(std::forward<Fn>(callback)) {}

  scope_exit(scope_exit&& other)
      : callback_(std::move(other.callback_)), active_(other.active_) {
    other.release();
  }

  scope_exit(scope_exit const& other) = delete;
  scope_exit& operator=(scope_exit const& other) = delete;
  scope_exit& operator=(scope_exit&& other) = delete;

  ~scope_exit() {
    if (active_) {
      callback_();
    }
  }

  void release() { active_ = false; }

private:
  Callback callback_;
  bool active_{true};
};

template<typename Fn>
scope_exit(Fn) -> scope_exit<Fn>;

} // namespace emb
