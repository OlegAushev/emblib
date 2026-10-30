#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>

#include "../types.hpp"

#include <emb/i18n.hpp>

namespace emb {
namespace can {
namespace canopen {
namespace detail {

// Returns "the signal at bit 3" for the signal whose first bit is `start`.
constexpr std::string signal_name(unsigned start)
{
  std::string out = "the signal at bit ";
  if (start >= 100) out += static_cast<char>('0' + start / 100);
  if (start >= 10) out += static_cast<char>('0' + start / 10 % 10);
  out += static_cast<char>('0' + start % 10);
  return out;
}

// Whether `value` fits a signal of `width` bits, from 1 to 64, that reads as a
// signed number if `is_signed` and as an unsigned one otherwise.
constexpr bool fits_width(std::int64_t value, unsigned width, bool is_signed)
{
  if (width >= 64) return is_signed || value >= 0;
  if (is_signed) {
    auto const half = std::int64_t{1} << (width - 1);
    return -half <= value && value < half;
  }
  return value >= 0 && (static_cast<std::uint64_t>(value) >> width) == 0;
}

// Returns the first rule of the rows of a PDO that `rows` break, or an empty
// string if they break none, for a `static_assert` to show, as `od_check` does
// for the rows of a dictionary. `rows` is an array or an `std::array` of the
// rows of one PDO (`some_pdo_rows`), and a rule names a signal by its first
// bit. Each signal is from 1 to as many bits wide as the type of its member
// (`max_width`), lies within the 64 bits of the frame and holds the lowest
// and the highest value that its member names, as its encoding reads them; no
// two signals overlap, and no two map one member, as their getters tell: a
// counter has the getter of its member. A PDO has at most one counter, of one
// to eight bits, as `rolling_counter` and `node_link` count. A signal of the
// role `value` has a name and a label in each language, and a counter none.
// The form of the name and the texts of the label are for the rules of the
// description to check, as those of the rows of a dictionary are.
template<typename Rows>
constexpr std::string pdo_check(Rows const& rows)
{
  auto counters = 0uz;
  for (auto i = 0uz; i < std::size(rows); ++i) {
    auto const& row = rows[i];
    auto const& bits = row.bits;
    auto const& codec = row.codec;

    if (bits.width < 1 || bits.width > codec.max_width) {
      return "pdo: "
           + signal_name(bits.start)
           + " must be from 1 to the bits of its member's type";
    }
    if (bits.start + bits.width > 64) {
      return "pdo: " + signal_name(bits.start) + " must lie within the 64 bits";
    }
    bool const is_signed = codec.encoding == pdo_encoding::signed_;
    if (!fits_width(codec.lowest_named, bits.width, is_signed)
        || !fits_width(codec.highest_named, bits.width, is_signed)) {
      return "pdo: "
           + signal_name(bits.start)
           + " is too narrow for the named values";
    }
    for (auto j = 0uz; j < i; ++j) {
      auto const& other = rows[j];
      if (bits.start < other.bits.start + other.bits.width
          && other.bits.start < bits.start + bits.width) {
        return "pdo: "
             + signal_name(bits.start)
             + " overlaps "
             + signal_name(other.bits.start);
      }
      if (codec.get == other.codec.get) {
        return "pdo: "
             + signal_name(bits.start)
             + " maps the member of "
             + signal_name(other.bits.start);
      }
    }
    bool const counter = codec.role == pdo_role::counter;
    if (counter && ++counters > 1) return "pdo: a PDO has at most one counter";
    if (counter && bits.width > 8) {
      return "pdo: "
           + signal_name(bits.start)
           + " is a counter and takes one to eight bits";
    }
    if (counter && (!row.name.empty() || !row.label.empty())) {
      return "pdo: "
           + signal_name(bits.start)
           + " is a counter and has no name or label";
    }
    if (!counter && (row.name.empty() || !i18n::is_complete(row.label))) {
      return "pdo: " + signal_name(bits.start) + " needs a name and a label";
    }
  }
  return {};
}

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
