#pragma once

#include <utility>

namespace emb {

template<typename Signature>
class delegate;

template<typename R, typename... Args>
class delegate<R(Args...)> {
  using Stub = R (*)(void*, Args...);

  void* obj_ = nullptr;
  Stub stub_ = nullptr;

  constexpr delegate(void* obj, Stub stub) noexcept : obj_(obj), stub_(stub) {}

  template<auto F>
  static R fn_stub(void*, Args... args) {
    return F(std::forward<Args>(args)...);
  }
  template<auto M, typename T>
  static R memfn_stub(void* p, Args... args) {
    return (static_cast<T*>(p)->*M)(std::forward<Args>(args)...);
  }
  template<auto M, typename T>
  static R const_memfn_stub(void* p, Args... args) {
    return (static_cast<T const*>(p)->*M)(std::forward<Args>(args)...);
  }

public:
  constexpr delegate() noexcept = default;

  template<auto F>
  static delegate bind() {
    return {nullptr, &fn_stub<F>};
  }

  template<auto M, typename T>
  static delegate bind(T* obj) {
    return {obj, &memfn_stub<M, T>};
  }

  template<auto M, typename T>
  static delegate bind(T const* obj) {
    return {const_cast<T*>(obj), &const_memfn_stub<M, T>};
  }

  explicit operator bool() const noexcept {
    return stub_ != nullptr;
  }

  R operator()(Args... args) const {
    return stub_(obj_, std::forward<Args>(args)...);
  }

  friend bool operator==(delegate lhs, delegate rhs) noexcept {
    return lhs.obj_ == rhs.obj_ && lhs.stub_ == rhs.stub_;
  }
  friend bool operator!=(delegate lhs, delegate rhs) noexcept {
    return !(lhs == rhs);
  }
};

} // namespace emb
