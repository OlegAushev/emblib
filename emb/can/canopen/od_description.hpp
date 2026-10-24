#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "detail/od_check.hpp"
#include "detail/od_describe.hpp"
#include "od.hpp"
#include "od_dictionary.hpp"

#include <emb/meta/fixed_string.hpp>
#include <emb/panic.hpp>

namespace emb {
namespace can {
namespace canopen {

// Returns the description, for a host, of the dictionary that
// `make_dictionary<Rows, Areas, Groups>()` makes, on the device named
// `Device`: a `std::array<char, N>` of exactly its text, in JSON Lines. Its
// first line is the header,
//
//   {"type":"header","format":"emb-canopen-od","version":1,
//    "device":"ADPT-ETK Inverter","fingerprint":"0x806A81B2"}
//
// on one line, the fingerprint being that of the dictionary
// (`od_dictionary::fingerprint`), the hash of every line after the header.
// A line for each group follows, then one for each label of a value, by
// table of values, then one for each member of each layout of a record, then
// one for each object, array or record, by key (`detail::od_describe`); a
// member, an object, an array or a record whose values a table names refers
// to it by its number. The text is canonical: no spaces between the tokens of
// a line, keys in a fixed order, no escapes in strings, which the rules of
// `detail::od_check` keep free of what would need one, hexadecimal numbers as
// strings of fixed width, decimal numbers without leading zeros, flags only
// when set, and a newline after every line. Two builds of the same dictionary
// thus describe it byte for byte the same.
//
// `Rows`, `Areas` and `Groups` must be those of `make_dictionary`, and the
// program is ill-formed if they break a rule of `detail::od_check`, if
// `Device` is empty or not UTF-8 without quotation marks, backslashes or
// control characters. An application keeps the description in a section of
// the image that is not loaded, from which the build extracts it.
template<auto const& Rows,
         auto const& Areas,
         auto const& Groups,
         fixed_string Device>
  requires some_od_rows<std::remove_reference_t<decltype(Rows)>>
        && some_od_areas<std::remove_reference_t<decltype(Areas)>>
        && some_od_groups<std::remove_reference_t<decltype(Groups)>>
consteval auto make_description()
{
  static_assert(detail::od_check(Rows, Areas, Groups).empty(),
                detail::od_check(Rows, Areas, Groups));
  static_assert(!Device.empty() && detail::is_verbatim_text(Device.view()),
                "od: the name of the device must not be empty and must be "
                "UTF-8 without quotation marks, backslashes or control "
                "characters");

  constexpr std::size_t size = [] {
    detail::od_counting_sink sink;
    detail::put_header(sink, Device.view(), 0);
    detail::od_describe(sink, Rows, Areas, Groups);
    return sink.size;
  }();

  // The body is written after a header of the right size, then the header
  // again, with the hash of the body.
  constexpr auto description = [] {
    std::array<char, size> text{};
    detail::od_writing_sink<size> body{text};
    detail::put_header(body, Device.view(), 0);
    body.hashing = {};
    detail::od_describe(body, Rows, Areas, Groups);
    detail::od_writing_sink<size> header{text};
    detail::put_header(header, Device.view(), body.hashing.hash);
    return text;
  }();
  static_assert(description.back() == '\n',
                "od: a description ends with a newline");
  static_assert(std::ranges::find(description, '\0') == description.end(),
                "od: a description holds no NUL");
  return description;
}

// Returns the fingerprint that the header of `description` states, as
// `make_description` writes it, for a `static_assert` that it is the
// dictionary's. If the text holds no fingerprint, constant evaluation fails
// and a call at run time terminates (`EMB_PANIC`).
constexpr std::uint32_t od_description_fingerprint(std::string_view description)
{
  constexpr std::string_view key = "\"fingerprint\":\"0x";
  auto const at = description.find(key);
  if (at == std::string_view::npos
      || at + key.size() + 8 > description.size()) {
    EMB_PANIC("od: the text holds no fingerprint");
  }
  std::uint32_t fingerprint = 0;
  for (auto const c : description.substr(at + key.size(), 8)) {
    auto const digit = c >= '0' && c <= '9' ? c - '0'
                     : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                            : -1;
    if (digit < 0) EMB_PANIC("od: the text holds no fingerprint");
    fingerprint = fingerprint * 16 + static_cast<std::uint32_t>(digit);
  }
  return fingerprint;
}

// Equivalent to `od_description_fingerprint` of a view of `description`.
template<std::size_t N>
constexpr std::uint32_t
od_description_fingerprint(std::array<char, N> const& description)
{
  return od_description_fingerprint(
      std::string_view{description.data(), description.size()});
}

} // namespace canopen
} // namespace can
} // namespace emb
