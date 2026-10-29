#pragma once

#include <array>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "od.hpp"
#include "types.hpp"

#include <emb/can.hpp>
#include <emb/i18n.hpp>

namespace emb {
namespace can {
namespace canopen {

// The scoped enumeration `pdo_direction` defines which way a PDO goes: `tx`, a
// TPDO, which the device sends, or `rx`, an RPDO, which it receives.
enum class pdo_direction : std::uint8_t {
  tx,
  rx
};

// The struct `od_pdo` describes a PDO as the server sets its slot up and the
// description of the dictionary states it: `name`, by what the PDO carries
// rather than by its slot, the first segment of the paths of its signals; its
// title for people, in each language; its direction; its number, that of its
// slot, from 1; its COB-ID, of the predefined form, a base + the node-ID, or
// custom; and `period`, at which a TPDO goes, or `timeout`, after which an
// RPDO that has not come is missed, 0 if the server does not watch it. A TPDO
// has no timeout and an RPDO no period. Like a row of a dictionary, it exists
// at compile time only, and `pdo_of` binds it to what the PDO carries.
struct od_pdo {
  std::string_view name;
  i18n title;
  pdo_direction direction;
  std::size_t number;
  pdo_id cob_id;
  std::chrono::milliseconds period{0};
  std::chrono::milliseconds timeout{0};
};

// The struct `od_signal` is a signal of a PDO as the description reads it: a
// row of the PDO (`pdo_row`) without its thunks, with the role, the encoding,
// the unit, the step `scale_num` / `scale_den` and the table of values of its
// codec.
struct od_signal {
  pdo_bits bits;
  od_label label;
  pdo_role role;
  pdo_encoding encoding;
  std::string_view unit;
  std::int64_t scale_num = 1;
  std::int64_t scale_den = 1;
  std::span<od_value_label const> value_labels;
};

// The struct `od_mapped_pdo` is a PDO with what it carries, as the description
// reads it: the PDO and the signals of its rows. A `mapped_pdo` converts to
// it.
struct od_mapped_pdo {
  od_pdo const& pdo;
  std::span<od_signal const> signals;

  // Returns what the PDO carries: the signals of its rows.
  constexpr pdo_mapping mapping() const
  {
    return pdo_mapping::signals;
  }
};

namespace detail {

// The signals of the rows `Rows`, in the order of the rows: each row without
// its thunks (`od_signal`). The members of a row are dependent names here, so
// the description of PDOs needs no codec, which includes it.
template<auto const& Rows>
inline constexpr auto od_signals_of = [] {
  std::array<od_signal, std::size(Rows)> signals{};
  for (auto i = 0uz; i < std::size(Rows); ++i) {
    auto const& codec = Rows[i].codec;
    signals[i] = {.bits = Rows[i].bits,
                  .label = Rows[i].label,
                  .role = codec.role,
                  .encoding = codec.encoding,
                  .unit = codec.unit,
                  .scale_num = codec.scale_num,
                  .scale_den = codec.scale_den,
                  .value_labels = codec.value_labels};
  }
  return signals;
}();

// Returns the base of the predefined COB-ID of the PDO numbered `number`, 1 to
// 4, that goes in the direction `direction`: its COB-ID less the node-ID,
// 180h + 100h (number - 1) for a TPDO and 200h + 100h (number - 1) for an
// RPDO.
constexpr id_t cob_base_of(pdo_direction direction, std::size_t number)
{
  auto const service =
      direction == pdo_direction::tx ? cob_type::tpdo : cob_type::rpdo;
  return cob_function_codes[std::to_underlying(service)]
       + static_cast<id_t>(0x100 * (number - 1));
}

} // namespace detail

// The class template `mapped_pdo` is a PDO whose type knows what it carries,
// `List`, the array of its rows, as `pdo_of` makes it. The server sets the
// slot of the PDO up from it, and it converts to the `od_mapped_pdo` that the
// description reads, so an application writes the PDO and its rows once, in
// a constant that both take.
template<auto const& List>
struct mapped_pdo {
  od_pdo pdo;

  constexpr operator od_mapped_pdo() const
  {
    return {.pdo = pdo, .signals = detail::od_signals_of<List>};
  }
};

namespace detail {

// The list of a `mapped_pdo`, which its type names.
template<typename Mapped>
struct mapped_pdo_traits {};

template<auto const& List>
struct mapped_pdo_traits<mapped_pdo<List>> {
  static constexpr auto const& list = List;
};

} // namespace detail

// The variable template `mapped_list` refers to `List`, what the
// `mapped_pdo<List>` `Mapped` carries, which its type alone knows. It is a
// reference, not a type: a concept takes
// `std::remove_cvref_t<decltype(mapped_list<Mapped>)>`.
template<auto const& Mapped>
inline constexpr auto const& mapped_list =
    detail::mapped_pdo_traits<std::remove_cvref_t<decltype(Mapped)>>::list;

// The struct `od_process_data` gives the description of a dictionary what it
// states beside the objects: the PDOs, each with what it carries, in any
// order. Like the rows, it exists at compile time only.
struct od_process_data {
  std::span<od_mapped_pdo const> pdos;
};

namespace detail {

// No process data, which `make_dictionary` and `make_description` take by
// default: their description is then that of the objects alone.
inline constexpr od_process_data od_no_process_data{};

} // namespace detail

// The concept `some_od_process_data<T>` is satisfied if and only if `T` is
// `od_process_data const`.
template<typename T>
concept some_od_process_data = std::same_as<T, od_process_data const>;

} // namespace canopen
} // namespace can
} // namespace emb
