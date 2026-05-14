#pragma once

#include <emb/assert.hpp>

#include <type_traits>
#include <utility>

namespace emb {

template<typename Signature>
class delegate;

template<typename R, typename... Args>
class delegate<R(Args...)> {
private:
  using Stub = R (*)(void*, Args...);

  void* obj_ = nullptr;
  Stub stub_ = nullptr;

  constexpr delegate(void* obj, Stub stub) noexcept : obj_(obj), stub_(stub) {}

  template<auto F>
  static constexpr R fn_stub(void*, Args... args) {
    return F(std::forward<Args>(args)...);
  }

  template<auto M, typename T>
  static constexpr R memfn_stub(void* p, Args... args) {
    return (static_cast<T*>(p)->*M)(std::forward<Args>(args)...);
  }

  template<auto M, typename T>
  static constexpr R const_memfn_stub(void* p, Args... args) {
    return (static_cast<T const*>(p)->*M)(std::forward<Args>(args)...);
  }

public:
  constexpr delegate() noexcept = default;

  template<auto F>
  [[nodiscard]] static constexpr delegate bind() {
    static_assert(
        std::is_invocable_r_v<R, decltype(F), Args...>,
        "emb::delegate: function not callable as R(Args...)"
    );
    return {nullptr, &fn_stub<F>};
  }

  template<auto M, typename T>
  [[nodiscard]] static constexpr delegate bind(T* obj) {
    static_assert(
        std::is_invocable_r_v<R, decltype(M), T*, Args...>,
        "emb::delegate: member function not callable as R(Args...)"
    );
    return {obj, &memfn_stub<M, T>};
  }

  template<auto M, typename T>
  [[nodiscard]] static constexpr delegate bind(T const* obj) {
    static_assert(
        std::is_invocable_r_v<R, decltype(M), T const*, Args...>,
        "emb::delegate: member function not callable as R(Args...)"
    );
    return {const_cast<T*>(obj), &const_memfn_stub<M, T>};
  }

  template<auto M, typename T>
  [[nodiscard]] static constexpr delegate bind(T& obj) {
    return bind<M>(&obj);
  }

  template<auto M, typename T>
  [[nodiscard]] static constexpr delegate bind(T const& obj) {
    return bind<M>(&obj);
  }

  // binding to a temporary would leave the delegate with a dangling pointer
  template<auto M, typename T>
  static delegate bind(T const&&) = delete;

  [[nodiscard]] explicit constexpr operator bool() const noexcept {
    return stub_ != nullptr;
  }

  constexpr R operator()(Args... args) const {
    ASSUME(stub_ != nullptr);
    return stub_(obj_, std::forward<Args>(args)...);
  }

  friend constexpr bool
  operator==(delegate const&, delegate const&) noexcept = default;
};

namespace detail {

template<typename T>
struct delegate_signature;

template<typename R, typename... Args>
struct delegate_signature<R (*)(Args...)> {
  using type = R(Args...);
};

template<typename R, typename T, typename... Args>
struct delegate_signature<R (T::*)(Args...)> {
  using type = R(Args...);
};

template<typename R, typename T, typename... Args>
struct delegate_signature<R (T::*)(Args...) const> {
  using type = R(Args...);
};

template<typename T>
using delegate_signature_t = typename delegate_signature<T>::type;

} // namespace detail

template<auto F>
[[nodiscard]] constexpr auto make_delegate() {
  return delegate<detail::delegate_signature_t<decltype(F)>>::template bind<
      F>();
}

template<auto M, typename T>
[[nodiscard]] constexpr auto make_delegate(T* obj) {
  return delegate<detail::delegate_signature_t<decltype(M)>>::template bind<M>(
      obj
  );
}

template<auto M, typename T>
[[nodiscard]] constexpr auto make_delegate(T const* obj) {
  return delegate<detail::delegate_signature_t<decltype(M)>>::template bind<M>(
      obj
  );
}

template<auto M, typename T>
[[nodiscard]] constexpr auto make_delegate(T& obj) {
  return make_delegate<M>(&obj);
}

template<auto M, typename T>
[[nodiscard]] constexpr auto make_delegate(T const& obj) {
  return make_delegate<M>(&obj);
}

// binding to a temporary would leave the delegate with a dangling pointer
template<auto M, typename T>
void make_delegate(T const&&) = delete;

} // namespace emb
