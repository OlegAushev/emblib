#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "od.hpp"
#include "od_value_cast.hpp"

#include <emb/meta/enum_carrier32.hpp>
#include <emb/meta/enum_labels.hpp>
#include <emb/meta/typelist.hpp>
#include <emb/meta/unit_symbol.hpp>

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
concept od_carried = od_scalar<T> || wraps_od_scalar<T> || od_enumeration<T>;

// Whether `T` is no enumeration, or one that labels its values, as an
// enumeration on the bus must.
template<typename T>
concept od_labelled = !std::is_enum_v<T> || enum_with_labels<T>;

// The labels of the values of the enumeration `E` as a table of values, for
// the binding of an object of it.
template<enum_with_labels E>
inline constexpr auto od_enum_value_labels = [] {
  constexpr auto labels = enum_labels_of<E>();
  std::array<od_value_label, labels.size()> out{};
  for (auto i = 0uz; i < labels.size(); ++i) {
    out[i] = {.value = std::to_underlying(labels[i].value()),
              .name = labels[i].name(),
              .title = labels[i].title()};
  }
  return out;
}();

// The table of the values of an object whose value is of type `T`: that of
// the labels of `T` if it is an enumeration, otherwise none.
template<typename T>
inline constexpr std::span<od_value_label const> od_value_labels_of{};

template<enum_with_labels E>
inline constexpr std::span<od_value_label const> od_value_labels_of<E> =
    od_enum_value_labels<E>;

// The values that the labels of the enumeration `E` name, in their carrier,
// in increasing order: all that a writer of `E` keeps of the labels.
template<enum_with_labels E>
inline constexpr auto od_enum_values = [] {
  constexpr auto labels = enum_labels_of<E>();
  std::array<enum_carrier32_t<E>, labels.size()> out{};
  for (auto i = 0uz; i < labels.size(); ++i) {
    out[i] = std::to_underlying(labels[i].value());
  }
  return out;
}();

// Whether `value`, in the carrier of the enumeration `E`, is a value that the
// labels of `E` name: one from the lowest to the highest if they name every
// value between, one of those they name otherwise.
template<enum_with_labels E>
constexpr bool od_names(enum_carrier32_t<E> value)
{
  constexpr auto& values = od_enum_values<E>;
  constexpr bool gapless = static_cast<std::int64_t>(values.back())
                             - static_cast<std::int64_t>(values.front())
                             + 1
                        == static_cast<std::int64_t>(values.size());
  if constexpr (gapless) {
    return values.front() <= value && value <= values.back();
  }
  else {
    return std::ranges::binary_search(values, value);
  }
}

// What a reader that returns `R` reads: the value, of `type`, and whether the
// reader may answer an abort instead (`fallible`), as one that returns
// `std::expected` does, or may have no value (`optional`), as one that returns
// `std::optional` does.
template<typename R>
struct od_read_value {
  using type = R;
  static constexpr bool fallible = false;
  static constexpr bool optional = false;
};

template<typename T>
struct od_read_value<std::expected<T, sdo_abort_code>> {
  using type = T;
  static constexpr bool fallible = true;
  static constexpr bool optional = false;
};

template<typename T>
struct od_read_value<std::optional<T>> {
  using type = T;
  static constexpr bool fallible = false;
  static constexpr bool optional = true;
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

// The elements of the array that a reader returns, by value or by const
// reference: their type and their number.
template<typename R>
struct od_elements : std::false_type {};

template<typename T, std::size_t N>
struct od_elements<std::array<T, N>> : std::true_type {
  using type = T;
  static constexpr std::size_t count = N;
};

template<typename T, std::size_t N>
struct od_elements<std::array<T, N> const&> : od_elements<std::array<T, N>> {};

template<typename R>
concept od_write_outcome =
    std::same_as<R, void> || std::same_as<R, od_write_result>;

// The concept `od_context_function<F>` is satisfied if and only if `F` is a
// function, or a lambda without captures, that takes a reference to a
// context and nothing else.
template<auto F>
concept od_context_function =
    od_function<F>
    && (od_args_t<F>::size == 1)
    && std::is_lvalue_reference_v<typelist_at_t<od_args_t<F>, 0>>;

// The context that the function `F` takes.
template<auto F>
using od_context_of = std::remove_reference_t<typelist_at_t<od_args_t<F>, 0>>;

} // namespace detail

// The concept `od_reader<F, Ctx>` is satisfied if and only if `F` is a
// function, or a lambda without captures, that takes nothing, `Ctx&` or
// `Ctx const&` and returns an od_value scalar, a type wrapping one (see
// `wraps_od_scalar`), an enumeration of at most 32 bits (see
// `od_enumeration`), `std::expected` of any of them with `sdo_abort_code`,
// or `std::optional` of any of them.
template<auto F, typename Ctx>
concept od_reader =
    detail::od_function<F>
    && detail::od_context_args<detail::od_args_t<F>, Ctx>
    && detail::od_carried<
        typename detail::od_read_value<detail::od_result_t<F>>::type>;

// The concept `od_writer<S, Ctx>` is satisfied if and only if `S` is a
// function, or a lambda without captures, that takes the value to write, by
// value or by const reference, optionally after `Ctx&` or `Ctx const&`, and
// returns `void` or `od_write_result`. The value is an od_value scalar, a
// type wrapping one, or an enumeration of at most 32 bits.
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

// The concept `od_string_reader<F, Ctx>` is satisfied if and only if `F` is
// a function, or a lambda without captures, that takes nothing, `Ctx&` or
// `Ctx const&` and returns `std::string_view` or `char const*`.
template<auto F, typename Ctx>
concept od_string_reader =
    detail::od_function<F>
    && detail::od_context_args<detail::od_args_t<F>, Ctx>
    && (std::same_as<detail::od_result_t<F>, std::string_view>
        || std::same_as<detail::od_result_t<F>, char const*>);

// The concept `od_array_reader<F, Ctx>` is satisfied if and only if `F` is a
// function, or a lambda without captures, that takes nothing, `Ctx&` or
// `Ctx const&` and returns `std::array`, by value or by const reference, of at
// most `od_array_capacity` od_value scalars, types wrapping one, or
// enumerations of at most 32 bits.
template<auto F, typename Ctx>
concept od_array_reader =
    detail::od_function<F>
    && detail::od_context_args<detail::od_args_t<F>, Ctx>
    && detail::od_elements<detail::od_result_t<F>>::value
    && detail::od_carried<
        typename detail::od_elements<detail::od_result_t<F>>::type>
    && (detail::od_elements<detail::od_result_t<F>>::count
        <= od_array_capacity);

// The concept `od_rw_pair<G, S, Ctx>` is satisfied if and only if `G` is a
// reader and `S` a writer whose values travel as the same scalar
// (`od_carrier_t`), even if one is wrapped and the other is not, or one is an
// enumeration and the other its carrier.
template<auto G, auto S, typename Ctx>
concept od_rw_pair =
    od_reader<G, Ctx>
    && od_writer<S, Ctx>
    && std::same_as<
        od_carrier_t<
            typename detail::od_read_value<detail::od_result_t<G>>::type>,
        od_carrier_t<
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

// Reads the value through `F`. An abort that `F` answers passes through, and
// an empty `std::optional` answers `no_data_available`, 0800 0024h: the
// object has no value to read now.
template<auto F, typename Ctx>
constexpr od_read_result od_read_thunk(Ctx& ctx, std::uint16_t)
{
  if constexpr (od_read_value<od_result_t<F>>::fallible) {
    return od_call<F>(ctx).transform(
        [](auto const& value) { return to_od_value(value); });
  }
  else if constexpr (od_read_value<od_result_t<F>>::optional) {
    auto const value = od_call<F>(ctx);
    if (!value) return std::unexpected(sdo_abort_code::no_data_available);
    return to_od_value(*value);
  }
  else {
    return to_od_value(od_call<F>(ctx));
  }
}

// Writes `value` through `S`. A value of an enumeration is checked in its
// carrier, before the enumeration is formed: one that its labels do not name
// answers `value_range_exceeded`, as 260 does for an enumeration of
// `std::uint8_t` that names 4.
template<auto S, typename Ctx>
constexpr od_write_result od_write_thunk(Ctx& ctx,
                                         std::uint16_t,
                                         od_value value)
{
  using value_type = typename od_written<od_args_t<S>, Ctx>::type;
  if constexpr (od_enumeration<value_type>) {
    auto const* const carried = std::get_if<od_carrier_t<value_type>>(&value);
    if (carried == nullptr) {
      return std::unexpected(sdo_abort_code::data_type_mismatch);
    }
    if (!od_names<value_type>(*carried)) {
      return std::unexpected(sdo_abort_code::value_range_exceeded);
    }
  }
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

// Runs the command `F`, if the value written is `Signature`, when it takes
// one.
template<auto F, typename Ctx, std::uint32_t... Signature>
constexpr od_write_result od_command_thunk(Ctx& ctx,
                                           std::uint16_t,
                                           [[maybe_unused]] od_value value)
{
  if constexpr (sizeof...(Signature) != 0) {
    auto const* const word = std::get_if<std::uint32_t>(&value);
    if (word == nullptr || ((*word != Signature) || ...)) {
      return std::unexpected(sdo_abort_code::data_store_error);
    }
  }

  if constexpr (std::same_as<od_result_t<F>, void>) {
    od_call<F>(ctx);
    return {};
  }
  else {
    return od_call<F>(ctx);
  }
}

// Returns the length of the string, as many characters as an array holds at
// most.
template<auto F, typename Ctx>
constexpr od_read_result od_string_count_thunk(Ctx& ctx, std::uint16_t)
{
  std::string_view const text = od_call<F>(ctx);
  return od_value{
      static_cast<std::uint8_t>(std::min(text.size(), od_array_capacity))};
}

// Returns the character of the string at `position`, or answers
// `subindex_not_found` past its length.
template<auto F, typename Ctx>
constexpr od_read_result od_string_char_thunk(Ctx& ctx, std::uint16_t position)
{
  std::string_view const text = od_call<F>(ctx);
  if (position >= std::min(text.size(), od_array_capacity)) {
    return std::unexpected(sdo_abort_code::subindex_not_found);
  }
  return od_value{static_cast<std::uint8_t>(text[position])};
}

// Returns `Count`, the number of elements of an array of a fixed length.
template<typename Ctx, std::size_t Count>
constexpr od_read_result od_array_count_thunk(Ctx&, std::uint16_t)
{
  return od_value{static_cast<std::uint8_t>(Count)};
}

// Returns the element at `position` of the array that `F` returns, or answers
// `subindex_not_found` past its end.
template<auto F, typename Ctx>
constexpr od_read_result od_array_element_thunk(Ctx& ctx,
                                                std::uint16_t position)
{
  if (position >= od_elements<od_result_t<F>>::count) {
    return std::unexpected(sdo_abort_code::subindex_not_found);
  }
  return to_od_value(od_call<F>(ctx)[position]);
}

inline constexpr std::string_view od_reader_shape =
    "od: a reader takes nothing or a reference to the context, and returns "
    "an od_value scalar, a type wrapping one, an enumeration of at most 32 "
    "bits, or std::expected or std::optional of any of them";

inline constexpr std::string_view od_writer_shape =
    "od: a writer takes the value, optionally after a reference to the "
    "context, and returns void or od_write_result";

inline constexpr std::string_view od_command_shape =
    "od: a command takes nothing or a reference to the context, and returns "
    "void or od_write_result";

inline constexpr std::string_view od_string_shape =
    "od: a string reader takes nothing or a reference to the context, and "
    "returns std::string_view or char const*";

inline constexpr std::string_view od_array_shape =
    "od: an array reader takes nothing or a reference to the context, and "
    "returns std::array, by value or by const reference, of at most 254 "
    "od_value scalars, types wrapping one or enumerations of at most 32 bits";

inline constexpr std::string_view od_unlabelled_enumeration =
    "od: an enumeration on the bus must be scoped and label its values "
    "(emb::enum_with_labels)";

inline constexpr std::string_view od_rw_type_disagreement =
    "od: the reader and the writer of an rw object disagree on the type";

inline constexpr std::string_view od_rw_unit_disagreement =
    "od: the reader and the writer of an rw object disagree on the unit";

inline constexpr std::string_view od_rw_values_disagreement =
    "od: the reader and the writer of an rw object disagree on the values";

// Whether the reader `G` and the writer `S` of an rw object, which travel as
// the same scalar (`od_rw_pair`), agree on the unit: the symbols of the types
// they read and write are equal, or one of them is empty, as that of a bare
// scalar is. A reader of `hz_f32` and a writer of `float` agree, on "Hz"; a
// reader of `rpm_f32` and a writer of `hz_f32` do not.
template<auto G, auto S, typename Ctx>
inline constexpr bool od_units_agree = [] {
  constexpr auto read =
      unit_symbol_of<typename od_read_value<od_result_t<G>>::type>();
  constexpr auto written =
      unit_symbol_of<typename od_written<od_args_t<S>, Ctx>::type>();
  return read.empty() || written.empty() || read == written;
}();

// Whether the reader `G` and the writer `S` of an rw object, which travel as
// the same scalar (`od_rw_pair`), agree on the values: both read and write the
// same enumeration, which names them, or neither reads or writes one. A reader
// of an enumeration and a writer of its carrier do not agree: the writer would
// take values the labels do not name.
template<auto G, auto S, typename Ctx>
inline constexpr bool od_values_agree = [] {
  using read_type = typename od_read_value<od_result_t<G>>::type;
  using written_type = typename od_written<od_args_t<S>, Ctx>::type;
  if constexpr (std::is_enum_v<read_type> || std::is_enum_v<written_type>) {
    return std::same_as<read_type, written_type>;
  }
  else {
    return true;
  }
}();

template<auto F, od_access Access>
struct read_binding {
  template<typename Ctx>
  consteval operator od_binding<Ctx>() const
  {
    static_assert(od_reader<F, Ctx>, od_reader_shape);
    if constexpr (od_reader<F, Ctx>) {
      using value_type = typename od_read_value<od_result_t<F>>::type;
      static_assert(od_labelled<value_type>, od_unlabelled_enumeration);
      if constexpr (od_labelled<value_type>) {
        return {.access = Access,
                .type = od_type_of<od_carrier_t<value_type>>,
                .read = &od_read_thunk<F, Ctx>,
                .unit = unit_symbol_of<value_type>(),
                .value_labels = od_value_labels_of<value_type>};
      }
      else {
        return {.access = Access,
                .type = od_data_type::uint32,
                .diagnosed = true};
      }
    }
    else {
      return {.access = Access,
              .type = od_data_type::uint32,
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
      static_assert(od_labelled<value_type>, od_unlabelled_enumeration);
      if constexpr (od_labelled<value_type>) {
        return {.access = od_access::wo,
                .type = od_type_of<od_carrier_t<value_type>>,
                .write = &od_write_thunk<S, Ctx>,
                .unit = unit_symbol_of<value_type>(),
                .value_labels = od_value_labels_of<value_type>};
      }
      else {
        return {.access = od_access::wo,
                .type = od_data_type::uint32,
                .diagnosed = true};
      }
    }
    else {
      return {.access = od_access::wo,
              .type = od_data_type::uint32,
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
        od_rw_type_disagreement);
    if constexpr (od_rw_pair<G, S, Ctx>) {
      using read_type = typename od_read_value<od_result_t<G>>::type;
      using written_type = typename od_written<od_args_t<S>, Ctx>::type;
      constexpr bool agree = od_units_agree<G, S, Ctx>;
      static_assert(agree, od_rw_unit_disagreement);
      constexpr bool same_values = od_values_agree<G, S, Ctx>;
      static_assert(same_values, od_rw_values_disagreement);
      // Agreeing on the values, both read and write the same enumeration,
      // if any.
      constexpr bool labelled = !same_values || od_labelled<read_type>;
      static_assert(labelled, od_unlabelled_enumeration);

      if constexpr (same_values && od_labelled<read_type>) {
        constexpr auto read_unit = unit_symbol_of<read_type>();
        return {.access = od_access::rw,
                .type = od_type_of<od_carrier_t<read_type>>,
                .read = &od_read_thunk<G, Ctx>,
                .write = &od_write_thunk<S, Ctx>,
                .unit = read_unit.empty() ? unit_symbol_of<written_type>()
                                          : read_unit,
                .value_labels = od_value_labels_of<read_type>,
                .diagnosed = !agree};
      }
      else {
        return {.access = od_access::rw,
                .type = od_data_type::uint32,
                .diagnosed = true};
      }
    }
    else {
      return {.access = od_access::rw,
              .type = od_data_type::uint32,
              .diagnosed = true};
    }
  }
};

template<auto F, std::uint32_t... Signature>
struct command_binding {
  static_assert(sizeof...(Signature) <= 1,
                "od: a command takes at most one signature");

  template<typename Ctx>
  consteval operator od_binding<Ctx>() const
  {
    static_assert(od_command<F, Ctx>, od_command_shape);
    if constexpr (od_command<F, Ctx>) {
      return {.access = od_access::wo,
              .type = od_data_type::uint32,
              .write = &od_command_thunk<F, Ctx, Signature...>,
              .command = true,
              .signature = std::optional<std::uint32_t>{Signature...}};
    }
    else {
      return {.access = od_access::wo,
              .type = od_data_type::uint32,
              .command = true,
              .diagnosed = true};
    }
  }
};

template<auto F>
struct string_binding {
  template<typename Ctx>
  consteval operator od_binding<Ctx>() const
  {
    static_assert(od_string_reader<F, Ctx>, od_string_shape);
    if constexpr (od_string_reader<F, Ctx>) {
      return {.access = od_access::const_,
              .type = od_data_type::uint8,
              .read = &od_string_char_thunk<F, Ctx>,
              .count = &od_string_count_thunk<F, Ctx>,
              .text = true};
    }
    else {
      return {.access = od_access::const_,
              .type = od_data_type::uint8,
              .text = true,
              .diagnosed = true};
    }
  }
};

template<auto F>
struct array_binding {
  template<typename Ctx>
  consteval operator od_binding<Ctx>() const
  {
    static_assert(od_array_reader<F, Ctx>, od_array_shape);
    if constexpr (od_array_reader<F, Ctx>) {
      using elements = od_elements<od_result_t<F>>;
      using element_type = typename elements::type;
      static_assert(od_labelled<element_type>, od_unlabelled_enumeration);
      if constexpr (od_labelled<element_type>) {
        return {.access = od_access::ro,
                .type = od_type_of<od_carrier_t<element_type>>,
                .read = &od_array_element_thunk<F, Ctx>,
                .count = &od_array_count_thunk<Ctx, elements::count>,
                .unit = unit_symbol_of<element_type>(),
                .value_labels = od_value_labels_of<element_type>};
      }
      else {
        return {.access = od_access::ro,
                .type = od_data_type::uint32,
                .diagnosed = true};
      }
    }
    else {
      return {.access = od_access::ro,
              .type = od_data_type::uint32,
              .diagnosed = true};
    }
  }
};

} // namespace detail

// The variable templates below bind an object in an `od_row` to typed
// handlers. The object's type is the scalar the handler's value travels as
// (`od_carrier_t`), its unit the symbol of the type of that value
// (`emb::unit_symbol_of`), if it names one, its table of values that of the
// labels of the value's type, if it is an enumeration, which must label its
// values (`emb::enum_with_labels`), and its access is the builder's; the
// row's context type is deduced from the row. A handler that does not fit its
// builder is reported at the row. A writer of an enumeration gets only the
// values its labels name: another value answers `value_range_exceeded`,
// 0609 0030h, and another type `data_type_mismatch`, 0607 0010h. A reader of
// an object may return `std::optional`: an empty one answers
// `no_data_available`, 0800 0024h, and the object's type, unit and table are
// those of the value it holds; the elements of an array may not be optional.
//
// `od_ro<F>` and `od_const<F>` bind a reader (`od_reader`), `od_wo<S>` a
// writer (`od_writer`), `od_rw<G, S>` a reader and a writer of the same
// scalar (`od_rw_pair`) whose units agree (`detail::od_units_agree`) and
// that read and write the same enumeration or none (`detail::od_values_agree`),
// `od_exec<F>` a command (`od_command`), a `uint32` object written to run it,
// whose value the command does not get, and `od_string<F>` a string
// (`od_string_reader`) as an array of its characters: subindex 00h reads its
// length, and each subindex after it one `uint8` character, up to
// `od_array_capacity` of them. A command and an array of characters have no
// unit, and their bindings are marked `command` and `text`. `od_ro_array<F>`
// binds an array that a reader returns (`od_array_reader`) the same way:
// subindex 00h reads the number of its elements, and each subindex after it
// one element, read-only, of the scalar the elements travel as.
// `od_exec<F, Signature>` binds a command that runs only if the value written
// is `Signature`, and answers any other with `data_store_error`, 0800 0020h,
// as CiA 301 asks of the objects that store parameters and restore their
// defaults (`od_save_signature`, `od_load_signature`); its binding keeps
// `Signature` as its `signature`.
template<auto F>
inline constexpr detail::read_binding<F, od_access::ro> od_ro{};

template<auto F>
inline constexpr detail::read_binding<F, od_access::const_> od_const{};

template<auto S>
inline constexpr detail::write_binding<S> od_wo{};

template<auto G, auto S>
inline constexpr detail::rw_binding<G, S> od_rw{};

template<auto F, std::uint32_t... Signature>
inline constexpr detail::command_binding<F, Signature...> od_exec{};

// The signatures that CiA 301 asks of a write to 1010h, which stores the
// parameters ("save"), and to 1011h, which restores their defaults ("load"):
// the ASCII of the word, first character in the lowest byte.
inline constexpr std::uint32_t od_save_signature = 0x65766173;
inline constexpr std::uint32_t od_load_signature = 0x64616F6C;

template<auto F>
inline constexpr detail::string_binding<F> od_string{};

template<auto F>
inline constexpr detail::array_binding<F> od_ro_array{};

} // namespace canopen
} // namespace can
} // namespace emb
