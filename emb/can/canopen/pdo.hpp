#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "detail/pdo_check.hpp"
#include "od.hpp"
#include "od_handlers.hpp"
#include "od_process_data.hpp"
#include "od_value_cast.hpp"
#include "types.hpp"

#include <emb/can.hpp>
#include <emb/math/scaled.hpp>
#include <emb/meta/enum_carrier32.hpp>
#include <emb/meta/enum_labels.hpp>
#include <emb/meta/unit_symbol.hpp>

namespace emb {
namespace can {
namespace canopen {

namespace detail {

// The class template `pdo_member_codec` describes how one member of a message
// of type `Message` travels as the bits of a signal: the core of the codec of
// a PDO, which of the dictionary knows only `od_names`. `get` returns the raw
// value of the member, its bits sign-extended to 64 if it reads as signed,
// and `set` stores the raw value of a signal in the member; `takes`, if set,
// tells whether the member accepts a raw value, as an enumeration accepts only
// the values that its labels name, and a member without it accepts any value
// that its signal holds. `encoding` is how the signal reads, `max_width` the
// bits of the member's type, which a signal may not exceed, and
// `lowest_named` and `highest_named` the lowest and the highest values that an
// enumeration names, which its signal must hold.
template<typename Message>
struct pdo_member_codec {
  using message_type = Message;

  std::uint64_t (*get)(Message const&) = nullptr;
  void (*set)(Message&, std::uint64_t) = nullptr;
  bool (*takes)(std::uint64_t) = nullptr;
  pdo_encoding encoding = pdo_encoding::unsigned_;
  std::uint8_t max_width = 0;
  std::int64_t lowest_named = 0;
  std::int64_t highest_named = 0;
};

} // namespace detail

// The class template `pdo_codec` is the codec of a member of a message of type
// `Message` that a PDO carries (`detail::pdo_member_codec`), with what the
// description of the PDO says of the member: its role, the symbol of the unit
// of its type, empty if it has none, its step `scale_num` / `scale_den`, the
// physical value of one raw step, and the table of its named values, empty if
// it names none. A builder makes it from the member (`pdo_map`,
// `pdo_map_element`, `pdo_map_counter`), and the type of the member alone
// gives what the description says: an `emb::scaled` the symbol of its
// quantity (`emb::unit_symbol_of`) and its step, an enumeration the table of
// its labels, the one that an object of it has in the dictionary, and any
// other type the symbol of its own unit, if it names one, and a step of 1.
// Like a row, a codec exists at compile time only.
template<typename Message>
struct pdo_codec : detail::pdo_member_codec<Message> {
  pdo_role role = pdo_role::value;
  std::string_view unit{};
  std::int64_t scale_num = 1;
  std::int64_t scale_den = 1;
  std::span<od_value_label const> value_labels{};
};

// The class template `pdo_row` is one signal of a PDO that carries a message
// of type `Message`, as an application writes it: where its bits lie, its
// label and the codec of the member it carries, which a builder gives:
//
//   inline constexpr pdo_row<drive_command> drive_command_rows[] = {
//       {{16, 16},
//        {"torque_ref", {"Torque reference", "Задание момента"}},
//        pdo_map<&drive_command::torque_ref>},
//       {{56, 2}, {}, pdo_map_counter<&drive_command::counter>},
//   };
//
// An array of rows lays the PDO out, and `encode_pdo`, `decode_pdo` and
// `peek_pdo` work from it at compile time; the bits that no row covers are
// reserved. The label gives the name and the title of a signal of the role
// `value`, and a counter has none (`od_signal_label`); the PDO, which
// `pdo_of` names, is not named again. Rows exist only at compile time: no
// name, title or text of a rule reaches the image.
template<typename Message>
struct pdo_row {
  pdo_bits bits;
  od_signal_label label;
  pdo_codec<Message> codec;
};

namespace detail {

// The class `M` of a pointer to a member of type `V M::*`, `message_type`, and
// the type `V` of the member, `value_type`.
template<typename T>
struct member_pointer {};

template<typename V, typename M>
struct member_pointer<V M::*> {
  using message_type = M;
  using value_type = V;
};

// The message that the pointer to a member `Member` points into, the type of
// the member, and the type of the element `I` of the member, an array.
template<auto Member>
using message_of = typename member_pointer<decltype(Member)>::message_type;

template<auto Member>
using member_of = typename member_pointer<decltype(Member)>::value_type;

template<auto Member, std::size_t I>
using element_of =
    std::remove_cvref_t<decltype(std::declval<member_of<Member>&>()[I])>;

// Return the member `Member`, and the element `I` of the member `Member`, by
// reference from a message, const or not: lambdas without captures, of
// structural types, which `codec_of` takes as template arguments.
template<auto Member>
inline constexpr auto member_ref = [](auto& m) -> auto& { return m.*Member; };

template<auto Member, std::size_t I>
inline constexpr auto element_ref =
    [](auto& m) -> auto& { return (m.*Member)[I]; };

// Whether `T` is a specialization of `emb::detail::scaled`, the class template
// that `emb::scaled` names.
template<typename T>
inline constexpr bool is_scaled = false;

template<std::integral Raw, some_quantity Q, some_ratio Step>
inline constexpr bool is_scaled<emb::detail::scaled<Raw, Q, Step>> = true;

// Returns the raw bits of the integer `v`, sign-extended to 64 if `T` is
// signed. The casts are explicit, so that a member of `std::uint64_t` raises no
// -Wsign-conversion warning.
template<std::integral T>
constexpr std::uint64_t raw_of(T v)
{
  if constexpr (std::is_signed_v<T>) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
  }
  else {
    return static_cast<std::uint64_t>(v);
  }
}

// How a signal reads that holds the raw value of the integer type `T`.
template<std::integral T>
inline constexpr pdo_encoding encoding_of =
    std::is_signed_v<T> ? pdo_encoding::signed_ : pdo_encoding::unsigned_;

// The bits of an object of type `T`.
template<typename T>
inline constexpr std::uint8_t bits_of =
    static_cast<std::uint8_t>(8 * sizeof(T));

inline constexpr std::string_view pdo_unlabelled_enumeration =
    "pdo: an enumeration in a PDO must be scoped, label its values "
    "(emb::enum_with_labels) and have at most 32 bits";

inline constexpr std::string_view pdo_uncarried_member =
    "pdo: a member in a PDO is a bool, an enumeration, an emb::scaled or an "
    "integer";

// Returns the core of the codec of a member of type `T` of a message of type
// `Message`, which `Ref` returns by reference from the message, const or not.
// A `bool` takes one bit; an enumeration, which must label its values and fit
// the carrier of the dictionary (`od_enumeration`), travels as its underlying
// value and reads as signed if that does, accepts only the values that its
// labels name (`od_names`), and its first and last labels give its lowest and
// highest values; an `emb::scaled` travels as its raw value, which
// `std::bit_cast` makes back into it; any other integer travels as it is.
// Compilation fails here, with a `static_assert`, for an enumeration that
// cannot travel and for any other type.
template<typename Message, typename T, auto Ref>
consteval pdo_member_codec<Message> codec_of()
{
  if constexpr (std::same_as<T, bool>) {
    return {.get = [](Message const& m) -> std::uint64_t {
              return Ref(m) ? 1u : 0u;
            },
            .set = [](Message& m, std::uint64_t raw) { Ref(m) = raw != 0; },
            .max_width = 1};
  }
  else if constexpr (std::is_enum_v<T>) {
    constexpr bool carried = enum_with_labels<T> && od_enumeration<T>;
    static_assert(carried, pdo_unlabelled_enumeration);
    if constexpr (carried) {
      using underlying = std::underlying_type_t<T>;
      constexpr auto labels = enum_labels_of<T>();
      return {.get =
                  [](Message const& m) {
                    return raw_of(std::to_underlying(Ref(m)));
                  },
              .set =
                  [](Message& m, std::uint64_t raw) {
                    Ref(m) = static_cast<T>(static_cast<underlying>(raw));
                  },
              .takes =
                  [](std::uint64_t raw) {
                    return od_names<T>(static_cast<enum_carrier32_t<T>>(raw));
                  },
              .encoding = encoding_of<underlying>,
              .max_width = bits_of<underlying>,
              .lowest_named = static_cast<std::int64_t>(
                  std::to_underlying(labels.front().value())),
              .highest_named = static_cast<std::int64_t>(
                  std::to_underlying(labels.back().value()))};
    }
    else {
      return {};
    }
  }
  else if constexpr (is_scaled<T>) {
    using raw_type = typename T::raw_type;
    return {.get = [](Message const& m) { return raw_of(Ref(m).raw()); },
            .set =
                [](Message& m, std::uint64_t raw) {
                  Ref(m) = std::bit_cast<T>(static_cast<raw_type>(raw));
                },
            .encoding = encoding_of<raw_type>,
            .max_width = bits_of<raw_type>};
  }
  else {
    static_assert(std::integral<T>, pdo_uncarried_member);
    if constexpr (std::integral<T>) {
      return {.get = [](Message const& m) { return raw_of(Ref(m)); },
              .set = [](Message& m,
                        std::uint64_t raw) { Ref(m) = static_cast<T>(raw); },
              .encoding = encoding_of<T>,
              .max_width = bits_of<T>};
    }
    else {
      return {};
    }
  }
}

// Returns the codec of a member of type `T` whose core is `core`, with what
// the description says of the member, which its type alone gives (see
// `pdo_codec`).
template<typename T, typename Message>
consteval pdo_codec<Message> described(pdo_member_codec<Message> const& core)
{
  pdo_codec<Message> codec{core};
  if constexpr (is_scaled<T>) {
    codec.unit = unit_symbol_of<typename T::quantity_type>();
    codec.scale_num = T::step_type::num;
    codec.scale_den = T::step_type::den;
  }
  else {
    codec.unit = unit_symbol_of<T>();
    codec.value_labels = od_value_labels_of<T>;
  }
  return codec;
}

} // namespace detail

// The variable templates `pdo_map`, `pdo_map_element` and `pdo_map_counter`
// are the builders of the codec of a row, each the codec of the member it
// names. `pdo_map<&Message::m>` maps the member `m`: a `bool`, an enumeration
// that labels its values (`emb::enum_with_labels`), an `emb::scaled` or an
// integer. `pdo_map_element<&Message::m, I>` maps the element `I` of the array
// `m`. `pdo_map_counter<&Message::m>` maps `m`, an unsigned integer and not a
// `bool`, as the counter of the frames of the PDO: it is `pdo_map<&Message::m>`
// with the role `counter`, so that the counter has the getter of its member,
// by which `peek_pdo` finds it and a rule of the rows sees it as `m`. A signed
// counter would read 2 in two bits as -2, where a host counts modulo
// 2^width. The getter of each member and of each element is a function of its
// own.
template<auto Member>
inline constexpr pdo_codec<detail::message_of<Member>> pdo_map =
    detail::described<detail::member_of<Member>>(
        detail::codec_of<detail::message_of<Member>,
                         detail::member_of<Member>,
                         detail::member_ref<Member>>());

template<auto Member, std::size_t I>
inline constexpr pdo_codec<detail::message_of<Member>> pdo_map_element =
    detail::described<detail::element_of<Member, I>>(
        detail::codec_of<detail::message_of<Member>,
                         detail::element_of<Member, I>,
                         detail::element_ref<Member, I>>());

template<auto Member>
inline constexpr pdo_codec<detail::message_of<Member>> pdo_map_counter = [] {
  using value_type = detail::member_of<Member>;
  static_assert(std::unsigned_integral<value_type>
                    && !std::same_as<value_type, bool>,
                "pdo: a counter is an unsigned integer");
  auto codec = pdo_map<Member>;
  codec.role = pdo_role::counter;
  return codec;
}();

namespace detail {

// Whether `Rows` is an array or an `std::array` of the rows of one PDO: by the
// kind of the array alone, since clang 22 remembers the result of a constraint
// on the type of a member of a row for every list it checks next.
template<typename Rows>
struct is_pdo_rows : std::false_type {};

template<typename Message, std::size_t N>
struct is_pdo_rows<pdo_row<Message>[N]> : std::true_type {};

template<typename Message, std::size_t N>
struct is_pdo_rows<std::array<pdo_row<Message>, N>> : std::true_type {};

} // namespace detail

// The concept `some_pdo_rows<Rows>` is satisfied if and only if `Rows` is an
// array or an `std::array` of the rows of a PDO, `pdo_row<Message>` of one
// message, as the functions of the codec take them:
// `std::remove_cvref_t<decltype(Rows)>` of their template argument.
template<typename Rows>
concept some_pdo_rows = detail::is_pdo_rows<Rows>::value;

// The type of the message that the rows `Rows` lay out.
template<auto const& Rows>
  requires some_pdo_rows<std::remove_cvref_t<decltype(Rows)>>
using pdo_message_t =
    typename std::remove_cvref_t<decltype(Rows[0].codec)>::message_type;

namespace detail {

static_assert(std::endian::native == std::endian::little,
              "pdo: the word of a frame is the bytes of its payload in "
              "little-endian order");

// Returns the payload as the 64-bit number whose byte j is byte j of the
// payload.
constexpr std::uint64_t to_word(payload_t const& payload)
{
  return std::bit_cast<std::uint64_t>(payload);
}

// Returns the payload whose byte j is byte j of `word`.
constexpr payload_t from_word(std::uint64_t word)
{
  return std::bit_cast<payload_t>(word);
}

// Returns the number whose `width` low bits are set, `width` from 1 to 64.
constexpr std::uint64_t low_mask(unsigned width)
{
  return width >= 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << width) - 1;
}

// Returns the raw value of the signal at `B` of `word`, sign-extended to 64
// bits if `E` reads it as signed.
template<pdo_bits B, pdo_encoding E>
constexpr std::uint64_t field_value(std::uint64_t word)
{
  auto const raw = (word >> B.start) & low_mask(B.width);
  if constexpr (E == pdo_encoding::signed_) {
    constexpr auto sign = std::uint64_t{1} << (B.width - 1);
    return (raw ^ sign) - sign;
  }
  else {
    return raw;
  }
}

// Returns true, after a `static_assert` that the rows `Rows` break no rule of
// the rows of a PDO (`pdo_check`), whose text it shows.
template<auto const& Rows>
consteval bool pdo_rows_hold()
{
  static_assert(pdo_check(Rows).empty(), pdo_check(Rows));
  return true;
}

// Whether the rows `Rows` break no rule of the rows of a PDO: compilation fails
// where it is named otherwise, with the rule broken, and the rules are checked
// once for each array of rows.
template<auto const& Rows>
inline constexpr bool pdo_rows_ok = pdo_rows_hold<Rows>();

// The position of the row of `Rows` that maps `Member` with `pdo_map` or
// `pdo_map_counter`, as their common getter tells, or the number of rows if
// none does.
template<auto Member, auto const& Rows>
inline constexpr std::size_t pdo_row_of = [] {
  constexpr auto get = pdo_map<Member>.get;
  for (auto i = 0uz; i < std::size(Rows); ++i) {
    if (Rows[i].codec.get == get) return i;
  }
  return std::size(Rows);
}();

} // namespace detail

// The bits of a frame that no row of `Rows` covers, set: the reserved bits of
// the PDO, which `encode_pdo` leaves 0 and `decode_pdo` refuses a frame for.
// The program is ill-formed if `Rows` breaks a rule of the rows of a PDO
// (`detail::pdo_check`).
template<auto const& Rows>
  requires some_pdo_rows<std::remove_cvref_t<decltype(Rows)>>
inline constexpr std::uint64_t pdo_reserved_mask = [] {
  static_assert(detail::pdo_rows_ok<Rows>);
  std::uint64_t covered = 0;
  for (auto const& row : Rows) {
    covered |= detail::low_mask(row.bits.width) << row.bits.start;
  }
  return ~covered;
}();

// The scoped enumeration `pdo_decode_refusal` defines why `decode_pdo` refuses
// a frame: a reserved bit is set, or a signal holds a value that its member
// does not accept, one that the labels of an enumeration do not name.
enum class pdo_decode_refusal : std::uint8_t {
  reserved_bit,
  unnamed_value
};

// The struct `pdo_decode_failure` tells why `decode_pdo` refused a frame: the
// reason, and with `unnamed_value` the position of the first row whose member
// does not accept the value of its signal; with `reserved_bit`, `row` means
// nothing.
struct pdo_decode_failure {
  pdo_decode_refusal reason;
  std::size_t row;

  friend constexpr bool operator==(pdo_decode_failure const&,
                                   pdo_decode_failure const&) = default;
};

// Returns the payload of a frame of the PDO that `Rows` lays out, carrying
// `message`: each row puts the low bits of its member's raw value into its
// signal, and the reserved bits are 0. The rows are read at compile time only,
// each a constant in the code it unrolls into, and no array of rows reaches
// the image. The program is ill-formed if `Rows` breaks a rule of the rows of a
// PDO (`detail::pdo_check`).
template<auto const& Rows>
  requires some_pdo_rows<std::remove_cvref_t<decltype(Rows)>>
constexpr payload_t encode_pdo(pdo_message_t<Rows> const& message)
{
  static_assert(detail::pdo_rows_ok<Rows>);
  std::uint64_t word = 0;
  [&]<std::size_t... I>(std::index_sequence<I...>) {
    (
        [&] {
          constexpr auto bits = Rows[I].bits;
          constexpr auto get = Rows[I].codec.get;
          word |= (get(message) & detail::low_mask(bits.width)) << bits.start;
        }(),
        ...);
  }(std::make_index_sequence<std::size(Rows)>{});
  return detail::from_word(word);
}

// Returns the message that `payload`, a frame of the PDO that `Rows` lays out,
// carries, or why the frame is refused (`pdo_decode_failure`): a reserved bit
// is set (`reserved_bit`), or a signal holds a value that its member does not
// accept (`unnamed_value`), as an enumeration accepts only the values that its
// labels name; the first such row, in the order of `Rows`, is the one named.
// Each row stores the raw value of its signal in its member, sign-extended if
// the signal reads as signed, and a member that no row maps is
// value-initialized. The frame is 8 bytes, as the server hands no shorter RPDO
// on. The program is ill-formed if `Rows` breaks a rule of the rows of a PDO
// (`detail::pdo_check`).
template<auto const& Rows>
  requires some_pdo_rows<std::remove_cvref_t<decltype(Rows)>>
constexpr std::expected<pdo_message_t<Rows>, pdo_decode_failure>
decode_pdo(payload_t const& payload)
{
  static_assert(detail::pdo_rows_ok<Rows>);
  auto const word = detail::to_word(payload);
  if ((word & pdo_reserved_mask<Rows>) != 0) {
    return std::unexpected(
        pdo_decode_failure{.reason = pdo_decode_refusal::reserved_bit,
                           .row = 0});
  }
  pdo_message_t<Rows> message{};
  auto refused = std::size(Rows);
  [&]<std::size_t... I>(std::index_sequence<I...>) {
    (... && [&] {
      constexpr auto bits = Rows[I].bits;
      constexpr auto encoding = Rows[I].codec.encoding;
      constexpr auto set = Rows[I].codec.set;
      constexpr auto takes = Rows[I].codec.takes;
      auto const raw = detail::field_value<bits, encoding>(word);
      if constexpr (takes != nullptr) {
        if (!takes(raw)) {
          refused = I;
          return false;
        }
      }
      set(message, raw);
      return true;
    }());
  }(std::make_index_sequence<std::size(Rows)>{});
  if (refused != std::size(Rows)) {
    return std::unexpected(
        pdo_decode_failure{.reason = pdo_decode_refusal::unnamed_value,
                           .row = refused});
  }
  return message;
}

// Returns the value of the member `Member` that `payload`, a frame of the PDO
// that `Rows` lays out, carries, without the checks of `decode_pdo`: whatever
// the reserved bits and the other signals hold, as a receiver needs to act on
// a signal before it checks the frame, or on a frame that it refuses. The
// value of an enumeration may be one that its labels do not name, for which
// `decode_pdo` refuses the frame. `Member` is a member of the message that a
// row maps with `pdo_map` or `pdo_map_counter`, as their getter tells: the
// program is ill-formed if no row does, as for a member that only rows of
// `pdo_map_element` map, or if `Rows` breaks a rule of the rows of a PDO
// (`detail::pdo_check`).
template<auto Member, auto const& Rows>
  requires some_pdo_rows<std::remove_cvref_t<decltype(Rows)>>
        && std::same_as<detail::message_of<Member>, pdo_message_t<Rows>>
constexpr auto peek_pdo(payload_t const& payload)
{
  static_assert(detail::pdo_rows_ok<Rows>);
  constexpr auto i = detail::pdo_row_of<Member, Rows>;
  static_assert(i < std::size(Rows), "pdo: no row of the PDO maps the member");
  if constexpr (i < std::size(Rows)) {
    constexpr auto bits = Rows[i].bits;
    constexpr auto encoding = Rows[i].codec.encoding;
    constexpr auto set = Rows[i].codec.set;
    pdo_message_t<Rows> message{};
    set(message, detail::field_value<bits, encoding>(detail::to_word(payload)));
    return message.*Member;
  }
  else {
    return detail::member_of<Member>{};
  }
}

// The class template `received_pdo` is a frame of an RPDO that the rows `Rows`
// lay out, as the server hands it to the handler of a slot set up from the
// description of the PDO (`server::setup_rpdo`): the type knows the rows, so
// that a handler takes the frames of its own PDO only. `peek` reads one
// member without the checks of `decode` (`peek_pdo`), as the emergency bit
// and the counter act on a frame that `decode` refuses, and `decode` returns
// the message, or why the frame is refused (`decode_pdo`). The frame holds 8
// bytes: a shorter one reaches no handler.
template<auto const& Rows>
  requires some_pdo_rows<std::remove_cvref_t<decltype(Rows)>>
class received_pdo {
public:
  constexpr explicit received_pdo(payload_t const& payload) : payload_(payload)
  {
  }

  template<auto Member>
  constexpr auto peek() const
  {
    return peek_pdo<Member, Rows>(payload_);
  }

  constexpr std::expected<pdo_message_t<Rows>, pdo_decode_failure>
  decode() const
  {
    return decode_pdo<Rows>(payload_);
  }

private:
  payload_t payload_;
};

// The type of the message that the PDO `Mapped`, a `mapped_pdo` of rows,
// carries: what the provider of its TPDO returns.
template<auto const& Mapped>
using mapped_message_t = pdo_message_t<mapped_list<Mapped>>;

namespace detail {

// The position of the row of `Rows` of the role `counter`, or the number of
// rows if the PDO has no counter.
template<auto const& Rows>
inline constexpr std::size_t pdo_counter_row = [] {
  for (auto i = 0uz; i < std::size(Rows); ++i) {
    if (Rows[i].codec.role == pdo_role::counter) return i;
  }
  return std::size(Rows);
}();

// The mask of the width of the counter of the rows `Rows`, by which a slot
// steps its counter of frames, or 0 if the PDO has no counter.
template<auto const& Rows>
inline constexpr std::uint8_t counter_mask_of = [] {
  constexpr auto row = pdo_counter_row<Rows>;
  if constexpr (row < std::size(Rows)) {
    return static_cast<std::uint8_t>(low_mask(Rows[row].bits.width));
  }
  else {
    return std::uint8_t{0};
  }
}();

} // namespace detail

// Returns the PDO `pdo` with what it carries, the message that the rows `Rows`
// lay out, as a `mapped_pdo<Rows>`, whose type knows the rows: the server
// sets the slot of the PDO up from it, and the description of the dictionary
// takes it, as an `od_mapped_pdo`, among the PDOs of its process data
// (`od_process_data`). An application writes the two once, in an
// `inline constexpr` variable of a named namespace that both take:
//
//   inline constexpr auto drive_status_pdo = pdo_of<drive_status_rows>(
//       {.name = "drive_status",
//        .title = {"Drive status", "Состояние привода"},
//        .direction = pdo_direction::tx,
//        .number = 1,
//        .cob_id = pdo_id::predefined(),
//        .period = 10ms});
//
// `pdo` is an argument rather than a template argument, as no template
// parameter takes the views of text that it holds. The program is ill-formed
// if `Rows` has no row, as a PDO carries at least one signal, or breaks a rule
// of the rows of a PDO (`detail::pdo_check`). The rules of the PDO and those
// of the names and the texts of its signals, which the description writes,
// are the description's (`detail::od_check`).
template<auto const& Rows>
  requires some_pdo_rows<std::remove_cvref_t<decltype(Rows)>>
consteval mapped_pdo<Rows> pdo_of(od_pdo const& pdo)
{
  static_assert(std::size(Rows) != 0, "pdo: a PDO carries at least one signal");
  static_assert(detail::pdo_rows_ok<Rows>);
  return {pdo};
}

} // namespace canopen
} // namespace can
} // namespace emb
