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
// rather than by its slot, which a host joins to the names of its signals;
// its title for people, in each language; its direction; its number, that
// of its slot, from 1; its COB-ID, of the predefined form, a base + the
// node-ID, or custom; and `period`, at which a TPDO goes, or `timeout`, after
// which an RPDO that has not come is missed, 0 if the server does not watch
// it. A TPDO has no timeout and an RPDO no period. Like a row of a
// dictionary, it exists at compile time only, and `pdo_of` binds it to what
// the PDO carries.
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
// codec. `name`, a segment unique in the PDO, which a host joins to the name
// of the PDO to find the signal, and the title, in each language, are those
// of the row: a signal of the role `value` has both, and a counter neither
// (`detail::pdo_check`). The unit is the codec's alone: a signal takes it from
// the type of its member, as its bits are the member's raw value, whose step
// the type gives.
struct od_signal {
  pdo_bits bits;
  std::string_view name;
  i18n title;
  pdo_role role;
  pdo_encoding encoding;
  std::string_view unit;
  std::int64_t scale_num = 1;
  std::int64_t scale_den = 1;
  std::span<od_value_info const> value_table;
};

// The struct `od_mapped_pdo` is a PDO with what it carries, as the description
// reads it: the PDO and either the signals of its rows or, of an MPDO in
// source address mode, the list of the objects that it sends, the other span
// empty. A `mapped_pdo` converts to it.
struct od_mapped_pdo {
  od_pdo const& pdo;
  std::span<od_signal const> signals;
  std::span<mpdo_scan const> scans;

  // Returns what the PDO carries: the objects of an MPDO in source address
  // mode if it has a list of them, else the signals of its rows. `pdo_of`
  // refuses an empty list of either, so the span that is not empty tells.
  constexpr pdo_mapping mapping() const
  {
    return scans.empty() ? pdo_mapping::signals : pdo_mapping::sam;
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
                  .name = Rows[i].name,
                  .title = Rows[i].title,
                  .role = codec.role,
                  .encoding = codec.encoding,
                  .unit = codec.unit,
                  .scale_num = codec.scale_num,
                  .scale_den = codec.scale_den,
                  .value_table = codec.value_table};
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
// `List`, the array of its rows or, of an MPDO in source address mode, the
// list of its objects (`mpdo_scan`), as `pdo_of` makes it. The server sets
// the slot of the PDO up from it, and it converts to the `od_mapped_pdo` that
// the description reads, so an application writes the PDO and what it
// carries once, in a constant that both take.
template<auto const& List>
struct mapped_pdo {
  od_pdo pdo;

  constexpr operator od_mapped_pdo() const
  {
    if constexpr (some_mpdo_scans<std::remove_cvref_t<decltype(List)>>) {
      return {.pdo = pdo, .signals = {}, .scans = List};
    }
    else {
      return {.pdo = pdo, .signals = detail::od_signals_of<List>, .scans = {}};
    }
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

// Returns the PDO `pdo`, an MPDO in source address mode, with what it carries,
// the objects of the list `Scans`, as a `mapped_pdo<Scans>`, whose type knows
// the list: the server sets the slot of the PDO up from it, with a source of
// the objects of the same list (`mpdo_scanner`), and the description of the
// dictionary takes it, as an `od_mapped_pdo`, among the PDOs of its process
// data. As with the rows of a PDO (`pdo_of` of `pdo.hpp`), an application
// writes the two once:
//
//   inline constexpr auto& statuses_objects =
//       od_statuses_scans<trouble::registry, 0x4000>;
//
//   inline constexpr auto statuses_pdo = pdo_of<statuses_objects>(
//       {.name = "statuses",
//        .title = {"Trouble statuses", "Статусы неисправностей"},
//        .direction = pdo_direction::tx,
//        .number = 1,
//        .cob_id = pdo_id::predefined(),
//        .period = 10ms});
//
// The program is ill-formed if `Scans` is empty, as an MPDO sends at least one
// object. That the objects are statuses, which the dictionary serves and lets
// read, each once, is a rule of the description (`detail::od_check`), which
// knows the dictionary.
template<auto const& Scans>
  requires some_mpdo_scans<std::remove_cvref_t<decltype(Scans)>>
consteval mapped_pdo<Scans> pdo_of(od_pdo const& pdo)
{
  static_assert(std::size(Scans) != 0,
                "pdo: an MPDO sends at least one object");
  return {pdo};
}

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
