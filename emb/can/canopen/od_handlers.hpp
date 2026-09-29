#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <type_traits>

#include "od.hpp"
#include "od_value_cast.hpp"

#include <emb/meta/typelist.hpp>

namespace emb {
namespace can {
namespace canopen {

namespace detail {

template<typename F>
struct od_signature : std::false_type {};

template<typename R, typename... Args, bool Noexcept>
struct od_signature<R (*)(Args...) noexcept(Noexcept)> : std::true_type {
  using result = R;
  using args = typelist<Args...>;
};

// A function, or a lambda without captures: anything unary plus turns into a
// pointer to a function.
template<auto F>
concept od_function = requires { +F; } && od_signature<decltype(+F)>::value;

template<auto F>
using od_result_t = typename od_signature<decltype(+F)>::result;

template<auto F>
using od_args_t = typename od_signature<decltype(+F)>::args;

template<typename T>
concept od_carried = od_scalar<T> || wraps_od_scalar<T>;

template<typename T>
struct od_scalar_of {
  using type = T;
};

template<wraps_od_scalar T>
struct od_scalar_of<T> {
  using type = typename T::value_type;
};

template<typename T>
using od_scalar_of_t = typename od_scalar_of<T>::type;

template<typename R>
struct od_read_value {
  using type = R;
  static constexpr bool fallible = false;
};

template<typename T>
struct od_read_value<std::expected<T, sdo_abort_code>> {
  using type = T;
  static constexpr bool fallible = true;
};

template<typename Args, typename Ctx>
concept od_context_args = std::same_as<Args, typelist<>>
                       || std::same_as<Args, typelist<Ctx&>>
                       || std::same_as<Args, typelist<Ctx const&>>;

template<typename U>
struct od_written_value {
  using type = std::remove_cvref_t<U>;
  static constexpr bool valid =
      od_carried<type>
      && (std::same_as<U, type> || std::same_as<U, type const&>);
};

template<typename Args, typename Ctx>
struct od_written {
  static constexpr bool valid = false;
};

template<typename U, typename Ctx>
struct od_written<typelist<U>, Ctx> : od_written_value<U> {};

template<typename U, typename Ctx>
struct od_written<typelist<Ctx&, U>, Ctx> : od_written_value<U> {};

template<typename U, typename Ctx>
struct od_written<typelist<Ctx const&, U>, Ctx> : od_written_value<U> {};

template<typename R>
concept od_write_outcome =
    std::same_as<R, void> || std::same_as<R, od_write_result>;

} // namespace detail

// The concept `od_reader<F, Ctx>` is satisfied if and only if `F` is a
// function, or a lambda without captures, that takes nothing, `Ctx&` or
// `Ctx const&` and returns an od_value scalar, a type wrapping one (see
// `wraps_od_scalar`), or `std::expected` of either with `sdo_abort_code`.
template<auto F, typename Ctx>
concept od_reader =
    detail::od_function<F>
    && detail::od_context_args<detail::od_args_t<F>, Ctx>
    && detail::od_carried<
        typename detail::od_read_value<detail::od_result_t<F>>::type>;

// The concept `od_writer<S, Ctx>` is satisfied if and only if `S` is a
// function, or a lambda without captures, that takes the value to write, by
// value or by const reference, optionally after `Ctx&` or `Ctx const&`, and
// returns `void` or `od_write_result`. The value is an od_value scalar or a
// type wrapping one.
template<auto S, typename Ctx>
concept od_writer = detail::od_function<S>
                 && detail::od_written<detail::od_args_t<S>, Ctx>::valid
                 && detail::od_write_outcome<detail::od_result_t<S>>;

// The concept `od_command<F, Ctx>` is satisfied if and only if `F` is a
// function, or a lambda without captures, that takes nothing, `Ctx&` or
// `Ctx const&` and returns `void` or `od_write_result`.
template<auto F, typename Ctx>
concept od_command = detail::od_function<F>
                  && detail::od_context_args<detail::od_args_t<F>, Ctx>
                  && detail::od_write_outcome<detail::od_result_t<F>>;

// The concept `od_text_reader<F, Ctx>` is satisfied if and only if `F` is a
// function, or a lambda without captures, that takes nothing, `Ctx&` or
// `Ctx const&` and returns `std::string_view` or `char const*`.
template<auto F, typename Ctx>
concept od_text_reader = detail::od_function<F>
                      && detail::od_context_args<detail::od_args_t<F>, Ctx>
                      && (std::same_as<detail::od_result_t<F>, std::string_view>
                          || std::same_as<detail::od_result_t<F>, char const*>);

// The concept `od_rw_pair<G, S, Ctx>` is satisfied if and only if `G` is a
// reader and `S` a writer whose values travel as the same scalar, even if one
// is wrapped and the other is not.
template<auto G, auto S, typename Ctx>
concept od_rw_pair =
    od_reader<G, Ctx>
    && od_writer<S, Ctx>
    && std::same_as<
        detail::od_scalar_of_t<
            typename detail::od_read_value<detail::od_result_t<G>>::type>,
        detail::od_scalar_of_t<
            typename detail::od_written<detail::od_args_t<S>, Ctx>::type>>;

namespace detail {

template<auto F, typename Ctx, typename... Values>
constexpr decltype(auto) od_call(Ctx& ctx, Values const&... values)
{
  if constexpr (od_args_t<F>::size == sizeof...(Values)) {
    return F(values...);
  }
  else {
    return F(ctx, values...);
  }
}

template<auto F, typename Ctx>
constexpr od_read_result od_read_thunk(Ctx& ctx, std::uint16_t)
{
  if constexpr (od_read_value<od_result_t<F>>::fallible) {
    return od_call<F>(ctx).transform(
        [](auto const& value) { return to_od_value(value); });
  }
  else {
    return to_od_value(od_call<F>(ctx));
  }
}

template<auto S, typename Ctx>
constexpr od_write_result od_write_thunk(Ctx& ctx,
                                         std::uint16_t,
                                         od_value value)
{
  using value_type = typename od_written<od_args_t<S>, Ctx>::type;
  auto const written = from_od_value<value_type>(value);
  if (!written) return std::unexpected(sdo_abort_code::data_type_mismatch);

  if constexpr (std::same_as<od_result_t<S>, void>) {
    od_call<S>(ctx, *written);
    return {};
  }
  else {
    return od_call<S>(ctx, *written);
  }
}

template<auto F, typename Ctx>
constexpr od_write_result od_command_thunk(Ctx& ctx, std::uint16_t, od_value)
{
  if constexpr (std::same_as<od_result_t<F>, void>) {
    od_call<F>(ctx);
    return {};
  }
  else {
    return od_call<F>(ctx);
  }
}

// Returns bytes [4 * word, 4 * word + 4) of the text as a little-endian
// uint32, with zeros past its end.
template<auto F, typename Ctx>
constexpr od_read_result od_text_thunk(Ctx& ctx, std::uint16_t word)
{
  std::string_view const text = od_call<F>(ctx);
  std::uint32_t chunk = 0;
  for (auto i = 0uz; i < 4; ++i) {
    auto const at = (4 * std::size_t{word}) + i;
    if (at < text.size()) {
      chunk |= std::uint32_t{static_cast<unsigned char>(text[at])} << (8 * i);
    }
  }
  return od_value{chunk};
}

inline constexpr std::string_view od_reader_shape =
    "od: a reader takes nothing or a reference to the context, and returns "
    "an od_value scalar, a type wrapping one, or std::expected of either";

inline constexpr std::string_view od_writer_shape =
    "od: a writer takes the value, optionally after a reference to the "
    "context, and returns void or od_write_result";

inline constexpr std::string_view od_command_shape =
    "od: a command takes nothing or a reference to the context, and returns "
    "void or od_write_result";

inline constexpr std::string_view od_text_shape =
    "od: a text reader takes nothing or a reference to the context, and "
    "returns std::string_view or char const*";

template<auto F, od_access Access>
struct read_binding {
  template<typename Ctx>
  consteval operator od_binding<Ctx>() const
  {
    static_assert(od_reader<F, Ctx>, od_reader_shape);
    if constexpr (od_reader<F, Ctx>) {
      using value_type = typename od_read_value<od_result_t<F>>::type;
      return {.access = Access,
              .type = od_type_of<od_scalar_of_t<value_type>>,
              .read = &od_read_thunk<F, Ctx>};
    }
    else {
      return {.access = Access,
              .type = od_value_type::uint32,
              .diagnosed = true};
    }
  }
};

template<auto S>
struct write_binding {
  template<typename Ctx>
  consteval operator od_binding<Ctx>() const
  {
    static_assert(od_writer<S, Ctx>, od_writer_shape);
    if constexpr (od_writer<S, Ctx>) {
      using value_type = typename od_written<od_args_t<S>, Ctx>::type;
      return {.access = od_access::wo,
              .type = od_type_of<od_scalar_of_t<value_type>>,
              .write = &od_write_thunk<S, Ctx>};
    }
    else {
      return {.access = od_access::wo,
              .type = od_value_type::uint32,
              .diagnosed = true};
    }
  }
};

template<auto G, auto S>
struct rw_binding {
  template<typename Ctx>
  consteval operator od_binding<Ctx>() const
  {
    static_assert(od_reader<G, Ctx>, od_reader_shape);
    static_assert(od_writer<S, Ctx>, od_writer_shape);
    static_assert(
        !(od_reader<G, Ctx> && od_writer<S, Ctx>) || od_rw_pair<G, S, Ctx>,
        "od: the reader and the writer of an rw object disagree "
        "on the type");
    if constexpr (od_rw_pair<G, S, Ctx>) {
      using value_type = typename od_read_value<od_result_t<G>>::type;
      return {.access = od_access::rw,
              .type = od_type_of<od_scalar_of_t<value_type>>,
              .read = &od_read_thunk<G, Ctx>,
              .write = &od_write_thunk<S, Ctx>};
    }
    else {
      return {.access = od_access::rw,
              .type = od_value_type::uint32,
              .diagnosed = true};
    }
  }
};

template<auto F>
struct command_binding {
  template<typename Ctx>
  consteval operator od_binding<Ctx>() const
  {
    static_assert(od_command<F, Ctx>, od_command_shape);
    if constexpr (od_command<F, Ctx>) {
      return {.access = od_access::wo,
              .type = od_value_type::exec,
              .write = &od_command_thunk<F, Ctx>};
    }
    else {
      return {.access = od_access::wo,
              .type = od_value_type::exec,
              .diagnosed = true};
    }
  }
};

template<auto F>
struct text_binding {
  template<typename Ctx>
  consteval operator od_binding<Ctx>() const
  {
    static_assert(od_text_reader<F, Ctx>, od_text_shape);
    if constexpr (od_text_reader<F, Ctx>) {
      return {.access = od_access::const_,
              .type = od_value_type::string,
              .read = &od_text_thunk<F, Ctx>};
    }
    else {
      return {.access = od_access::const_,
              .type = od_value_type::string,
              .diagnosed = true};
    }
  }
};

struct restore_default_binding {
  template<typename Ctx>
  consteval operator od_binding<Ctx>() const
  {
    return {.access = od_access::wo, .type = od_value_type::exec};
  }
};

} // namespace detail

// The variable templates below bind an object in an `od_row` to typed
// handlers. The object's type is the scalar the handler's value travels as,
// and its access is the builder's; the row's context type is deduced from the
// row. A handler that does not fit its builder is reported at the row.
//
// `od_ro<F>` and `od_const<F>` bind a reader (`od_reader`), `od_wo<S>` a
// writer (`od_writer`), `od_rw<G, S>` a reader and a writer of the same
// scalar (`od_rw_pair`), `od_exec<F>` a command (`od_command`), which gets
// no value, and `od_text<F>` a text (`od_text_reader`), which a client reads
// 4 bytes at a time up to and including its terminating NUL.
// `od_restore_default` binds 1011h:04, the key the server serves itself.
template<auto F>
inline constexpr detail::read_binding<F, od_access::ro> od_ro{};

template<auto F>
inline constexpr detail::read_binding<F, od_access::const_> od_const{};

template<auto S>
inline constexpr detail::write_binding<S> od_wo{};

template<auto G, auto S>
inline constexpr detail::rw_binding<G, S> od_rw{};

template<auto F>
inline constexpr detail::command_binding<F> od_exec{};

template<auto F>
inline constexpr detail::text_binding<F> od_text{};

inline constexpr detail::restore_default_binding od_restore_default{};

} // namespace canopen
} // namespace can
} // namespace emb
