#pragma once

#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace emb::pipe {

// A pipeline is a chain of steps written left to right with `|`. A step takes
// what the chain carries and returns what the next step gets:
//
//   ctx.I.dq = ctx.I.phase | clarke_transform
//                          | store(ctx.I.ab)
//                          | park_transform(sine, cosine);
template<typename Self>
struct pipeable {};

template<typename P>
concept some_pipeable =
    std::derived_from<std::remove_cvref_t<P>, pipeable<std::remove_cvref_t<P>>>;

// A step is invoked as const: a pipeline is a value that can be stored, copied
// and applied twice, so a step that has to change something holds a reference
// to it, the way store() does.
template<typename P, typename In>
concept pipeable_for = some_pipeable<P> && std::invocable<P const&, In>;

template<typename P, typename In>
  requires pipeable_for<P, In>
using output_t = std::invoke_result_t<P const&, In>;

// ---------------------------------------------------------------- composition

// Two steps piped together make a step, so a pipeline is a value in its own
// right: it can be named, stored next to the thing it belongs to, handed
// around and tested without a value to run through it.
template<typename... P>
class composed : public pipeable<composed<P...>> {
public:
  std::tuple<P...> steps;

  constexpr explicit composed(std::tuple<P...> s) : steps{std::move(s)} {}

  constexpr auto operator()(auto&& in) const
  {
    return run<0>(decltype(in)(in));
  }
private:
  // by value: an intermediate result belongs to the step that produced it, and
  // a reference to it would not outlive the step that comes next
  template<std::size_t I>
  constexpr auto run(auto&& v) const
  {
    if constexpr (I == sizeof...(P)) {
      return decltype(v)(v);
    }
    else {
      return run<I + 1>(std::get<I>(steps)(decltype(v)(v)));
    }
  }
};

namespace detail {

template<typename P>
constexpr auto steps_of(P p)
{
  return std::tuple<P>{std::move(p)};
}

template<typename... P>
constexpr auto steps_of(composed<P...> c)
{
  return std::move(c).steps;
}

template<typename... P>
constexpr auto make_composed(std::tuple<P...> steps)
{
  return composed<P...>{std::move(steps)};
}

} // namespace detail

// ------------------------------------------------------------------ operator|

// a value on the left, a step on the right: run it
template<typename In, some_pipeable P>
  requires(!some_pipeable<In> && std::invocable<P const&, In>)
constexpr decltype(auto) operator|(In&& in, P const& p)
{
  return p(std::forward<In>(in));
}

// two steps: compose them, flattened, so a chain of N steps is one type with N
// members rather than N nested ones
template<some_pipeable L, some_pipeable R>
[[nodiscard]] constexpr auto operator|(L l, R r)
{
  return detail::make_composed(std::tuple_cat(detail::steps_of(std::move(l)),
                                              detail::steps_of(std::move(r))));
}

// a step that cannot eat what the chain carries; here to say so plainly
// instead of leaving a wall of rejected candidates
template<typename In, some_pipeable P>
  requires(!some_pipeable<In> && !std::invocable<P const&, In>)
constexpr void operator|(In&&, P const&)
{
  static_assert(false,
                "emb::pipe: this step is not callable with the value the "
                "pipeline carries at that point");
}

// -------------------------------------------------------------------- lifting

// Lifts a plain callable -- a lambda, a function pointer -- into a step. This
// is the one way in for anything that does not derive from pipeable itself.
template<typename F>
class fn_t : public pipeable<fn_t<F>> {
  F f_;
public:
  constexpr explicit fn_t(F f) : f_{std::move(f)} {}

  constexpr decltype(auto) operator()(auto&& in) const
    requires std::invocable<F const&, decltype(in)>
  {
    return f_(decltype(in)(in));
  }
};

template<typename F>
[[nodiscard]] constexpr auto fn(F&& f)
{
  return fn_t<std::decay_t<F>>{std::forward<F>(f)};
}

// Binds the trailing arguments of a callable that takes more than one, leaving
// a step of one argument.
//
// An argument is copied where with() is written, and that point is not
// sequenced against the rest of the chain: in
//
//   x | store(m) | with(f, m)
//
// f is handed the value m held before the chain ran. Pass std::ref(m) to have
// the argument read at the moment the step runs instead.
template<typename F, typename... A>
class with_t : public pipeable<with_t<F, A...>> {
  F f_;
  std::tuple<A...> args_;
public:
  constexpr explicit with_t(F f, A... a)
      : f_{std::move(f)}, args_{std::move(a)...}
  {
  }

  constexpr decltype(auto) operator()(auto&& in) const
  {
    return std::apply(
        [&](auto const&... a) -> decltype(auto) {
          return f_(decltype(in)(in), a...);
        },
        args_);
  }
};

template<typename F, typename... A>
[[nodiscard]] constexpr auto with(F&& f, A&&... a)
{
  return with_t<std::decay_t<F>, std::decay_t<A>...>{std::forward<F>(f),
                                                     std::forward<A>(a)...};
}

// --------------------------------------------------------------- side effects

// Looks at what passes and hands it on unchanged.
template<typename F>
class tap_t : public pipeable<tap_t<F>> {
  F f_;
public:
  constexpr explicit tap_t(F f) : f_{std::move(f)} {}

  constexpr auto operator()(auto in) const
    requires std::invocable<F const&, decltype(in) const&>
  {
    f_(in);
    return in;
  }
};

template<typename F>
[[nodiscard]] constexpr auto tap(F&& f)
{
  return tap_t<std::decay_t<F>>{std::forward<F>(f)};
}

// Writes what passes into dest and hands it on unchanged. The step holds a
// reference, so a pipeline built with it must not outlive its destination.
template<typename T>
class store_t : public pipeable<store_t<T>> {
  T& dest_;
public:
  constexpr explicit store_t(T& dest) : dest_{dest} {}

  constexpr auto operator()(auto in) const
    requires std::assignable_from<T&, decltype(in) const&>
  {
    dest_ = in;
    return in;
  }
};

template<typename T>
[[nodiscard]] constexpr auto store(T& dest)
{
  return store_t<T>{dest};
}

} // namespace emb::pipe
