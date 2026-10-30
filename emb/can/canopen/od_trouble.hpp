#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "od.hpp"
#include "od_handlers.hpp"
#include "types.hpp"

#include <emb/i18n.hpp>
#include <emb/meta/enum_info.hpp>
#include <emb/meta/fixed_string.hpp>
#include <emb/meta/typelist.hpp>
#include <emb/trouble.hpp>

namespace emb {
namespace can {
namespace canopen {

// Returns the severity of a status that stands at `level`, as its member of
// the record of the statuses reads it: 0 if the status stands at no level,
// and the level + 1 if it stands, so that the lowest level reads 1.
template<trouble::level_like Level>
constexpr std::uint8_t severity_value_of(std::optional<Level> level)
{
  if (!level) return 0;
  return static_cast<std::uint8_t>(std::to_underlying(*level) + 1);
}

// The scale of severity of the statuses whose levels are `Level`, a table of
// values: `none`, 0, a status that stands at no level, then each enumerator
// of `Level` (`emb::enum_info_of`) at the severity of its level, the level + 1
// (`severity_value_of`). Each member of the record of the statuses past 00h
// takes its values from it.
template<enum_with_info Level>
inline constexpr auto od_severity_labels = [] {
  constexpr auto info = enum_info_of<Level>();
  std::array<od_value_label, info.size() + 1> out{};
  out[0] = {.value = severity_value_of<Level>(std::nullopt),
            .name = "none",
            .title = {"None", "Нет"}};
  for (auto i = 0uz; i < info.size(); ++i) {
    out[i + 1] = {.value = severity_value_of<Level>(info[i].value()),
                  .name = info[i].name(),
                  .title = info[i].title()};
  }
  return out;
}();

namespace detail {

// The statuses of a registry of type `Registry`, `status_list`, its levels,
// `level_type`, and the number of its levels, `level_count`, which its type
// names and the registry itself does not open: those of an
// `emb::trouble::registry`, or of a registry that a test puts in its place
// and specializes the trait for.
template<typename Registry>
struct od_registry_traits {};

template<typename StatusList, typename Level, std::size_t LevelCount>
struct od_registry_traits<trouble::registry<StatusList, Level, LevelCount>> {
  using status_list = StatusList;
  using level_type = Level;
  static constexpr std::size_t level_count = LevelCount;
};

// The concept `od_known_registry<R>` is satisfied if and only if
// `od_registry_traits` names the statuses and the levels of `R`.
template<typename R>
concept od_known_registry =
    requires { typename od_registry_traits<R>::status_list; };

// The concept `od_labelled_status<S>` is satisfied if and only if the status
// `S` states its name, `name`, and its title, `title`, which label its member
// of the record of the statuses.
template<typename S>
concept od_labelled_status = requires {
  { S::name } -> std::convertible_to<std::string_view>;
  { S::title } -> std::convertible_to<i18n>;
};

// Whether each status of the list states its name and its title.
template<typename... Statuses>
consteval bool od_statuses_labelled(typelist<Statuses...>)
{
  return (od_labelled_status<Statuses> && ...);
}

// Whether `Level` labels each of the `LevelCount` levels of a registry, 0 to
// LevelCount - 1, and no other value: a member of the record of the statuses
// would otherwise read a severity that the scale does not name.
template<typename Level, std::size_t LevelCount>
consteval bool od_levels_labelled()
{
  if constexpr (enum_with_info<Level>) {
    constexpr auto info = enum_info_of<Level>();
    if (info.size() != LevelCount) return false;
    for (auto i = 0uz; i < info.size(); ++i) {
      if (std::cmp_not_equal(std::to_underlying(info[i].value()), i)) {
        return false;
      }
    }
    return true;
  }
  else {
    return false;
  }
}

// The decimal digits of `Value`.
template<std::size_t Value>
inline constexpr auto od_decimal = [] {
  constexpr auto digits = [] {
    auto count = 1uz;
    for (auto left = Value / 10; left != 0; left /= 10) {
      ++count;
    }
    return count;
  }();
  fixed_string<digits + 1> out;
  auto rest = Value;
  for (auto i = digits; i > 0; --i) {
    out.chars[i - 1] = static_cast<char>('0' + rest % 10);
    rest /= 10;
  }
  return out;
}();

// The name and the titles of the stub of the retired status of the id `Id`:
// "retired_22", "Retired status 22" and "Снятый статус 22".
template<std::size_t Id>
inline constexpr auto od_retired_name = "retired_" + od_decimal<Id>;

template<std::size_t Id>
inline constexpr auto od_retired_title_en = "Retired status " + od_decimal<Id>;

template<std::size_t Id>
inline constexpr auto od_retired_title_ru = "Снятый статус " + od_decimal<Id>;

inline constexpr std::string_view od_unlabelled_levels =
    "od: the enumeration of the levels of the statuses must label each level "
    "of the registry, from 0, and no other value";

inline constexpr std::string_view od_too_many_statuses =
    "od: the ids of the statuses must be below 254: a status takes subindex "
    "id + 1, and the last is FEh";

inline constexpr std::string_view od_unlabelled_status =
    "od: each status must state its name and its title, which label its "
    "member";

// The binding of the record of the statuses of `Registry` (`od_statuses`):
// its layout, a template of the context, and the labels of its members.
template<auto& Registry>
struct statuses_binding {
  using traits = od_registry_traits<std::remove_cvref_t<decltype(Registry)>>;
  using status_list = typename traits::status_list;
  using level_type = typename traits::level_type;

  // One past the largest id: a member for each id, at subindex id + 1.
  static constexpr std::size_t id_space = trouble::id_space_v<status_list>;

  static_assert(od_levels_labelled<level_type, traits::level_count>(),
                od_unlabelled_levels);
  static_assert(id_space <= 0xFE, od_too_many_statuses);
  static_assert(od_statuses_labelled(status_list{}), od_unlabelled_status);

  // Whether a status of the list has the id, for each id.
  static constexpr std::array<bool, id_space> live = [] {
    std::array<bool, id_space> out{};
    trouble::for_each<status_list>([&]<typename S>(S) { out[S::id] = true; });
    return out;
  }();

  // -- Members of the record, whose arg is not used --

  template<typename Ctx>
  static constexpr od_read_result read_highest_subindex(Ctx&, std::uint16_t)
  {
    return od_value{static_cast<std::uint8_t>(id_space)};
  }

  // Reads the severity of `Status`, the one call the bridge makes to the
  // registry.
  template<typename Ctx, typename Status>
  static constexpr od_read_result read_severity(Ctx&, std::uint16_t)
  {
    return od_value{severity_value_of(Registry.severity(Status{}))};
  }

  // Reads 0, the stub of a retired status.
  template<typename Ctx>
  static constexpr od_read_result read_retired(Ctx&, std::uint16_t)
  {
    return od_value{std::uint8_t{0}};
  }

  // The fields of the record: 00h, then a reader of a severity at subindex
  // id + 1 of each status, and of 0 on a hole in the ids.
  template<typename Ctx>
  static constexpr auto layout = [] {
    std::array<od_field<Ctx>, id_space + 1> fields{};
    fields[0] = {.access = od_access::const_,
                 .type = od_data_type::uint8,
                 .read = &read_highest_subindex<Ctx>};
    for (auto i = 1uz; i < fields.size(); ++i) {
      fields[i] = {.access = od_access::ro,
                   .type = od_data_type::uint8,
                   .read = &read_retired<Ctx>};
    }
    trouble::for_each<status_list>([&]<typename S>(S) {
      fields[std::size_t{S::id} + 1].read = &read_severity<Ctx, S>;
    });
    return fields;
  }();

  // The labels of the members: that of 00h, then, at subindex id + 1, the
  // name and the title of the status of the id, or those of the stub of a
  // retired one; each member past 00h takes its values from the scale.
  static constexpr auto labels = [] {
    constexpr std::span<od_value_label const> scale =
        od_severity_labels<level_type>;
    std::array<od_field_label, id_space + 1> out{};
    out[0] = {.name = "highest_subindex",
              .title = {"Highest subindex", "Наибольший подындекс"}};
    [&]<std::size_t... Id>(std::index_sequence<Id...>) {
      (
          [&] {
            if constexpr (!live[Id]) {
              out[Id + 1] = {.name = od_retired_name<Id>.view(),
                             .title = {od_retired_title_en<Id>.view(),
                                       od_retired_title_ru<Id>.view()},
                             .value_labels = scale};
            }
          }(),
          ...);
    }(std::make_index_sequence<id_space>{});
    trouble::for_each<status_list>([&]<typename S>(S) {
      if constexpr (od_labelled_status<S>) {
        out[std::size_t{S::id} + 1] = {.name = S::name,
                                       .title = S::title,
                                       .value_labels = scale};
      }
    });
    return out;
  }();

  template<typename Ctx>
  consteval operator od_binding<Ctx>() const
  {
    return {.access = od_access::ro,
            .type = od_data_type::uint8,
            .layout = layout<Ctx>,
            .field_labels = labels};
  }
};

} // namespace detail

// The variable template `od_statuses<Registry>` binds, in an `od_row`, the
// record of the statuses of the trouble registry `Registry`
// (`emb::trouble::registry`): the severity of each status, which a host finds
// by the path of the record and the name of the status,
// "trouble.statuses.ov_dc". The row names the record with subindex 00h and
// gives its label, and the area of its index, of the class `status`, tells a
// host what the record is:
//
//   {{0x4000, 0x00}, {"trouble.statuses", {"Trouble statuses", "Статусы неисправностей"}}, od_statuses<trouble::registry>},
//
// Its members, by subindex:
//
//   00h       const u8  the highest subindex: the largest id + 1
//   id + 1    ro u8     the severity of the status of the id
//                       (`severity_value_of`): 0 if it stands at no level,
//                       and the level + 1 if it stands
//   hole + 1  ro u8     0: the stub of a retired status, on an id that no
//                       status of the list has
//
// A member of a status is named and titled by the status's `name` and
// `title`. The stub of a retired one is named "retired_<id>" and titled
// "Retired status <id>" and "Снятый статус <id>": an id is the address of its
// status, so a retired status leaves a hole in the ids rather than moving the
// statuses after it, and the stub keeps their members in place. Each member
// past 00h takes its values from the scale of severity
// (`od_severity_labels`), the stub as well.
//
// The bridge calls only `severity` of the registry, a status at a time, and
// takes the statuses and the levels from the type of the registry
// (`detail::od_registry_traits`). The enumeration of the levels must label
// each of its levels from 0 and no other value, as the scale names the
// severities a member reads; the ids must be below 254, as the last subindex
// is FEh; and each status must state its name and its title. A registry that
// breaks one fails to compile, at the row. Like the labels of the rows, the
// names and the titles of the members exist at compile time only: the image
// holds the layout and a reader for each status.
template<auto& Registry>
  requires detail::od_known_registry<std::remove_cvref_t<decltype(Registry)>>
inline constexpr detail::statuses_binding<Registry> od_statuses{};

namespace detail {

// The number of runs of live statuses in `live`, the ids that a status of the
// list has: of ids that follow each other.
template<std::size_t N>
consteval std::size_t od_status_runs(std::array<bool, N> const& live)
{
  auto runs = 0uz;
  for (auto id = 0uz; id < N; ++id) {
    if (live[id] && (id == 0 || !live[id - 1])) ++runs;
  }
  return runs;
}

} // namespace detail

// The variable template `od_statuses_scans<Registry, Index>` is the list of the
// objects of the record of the statuses of `Registry` at `Index`
// (`od_statuses`) that an MPDO in source address mode sends: a block
// `{Index, id + 1, n}` for each run of n live statuses, by id, so one block if
// no status is retired. Neither 00h nor the stub of a retired status is in
// it, as they tell a host nothing and would take frames of the round. An
// application takes the list as that of the PDO (`pdo_of`) and of its source
// (`mpdo_scanner`), and writes no number of statuses by hand:
//
//   inline constexpr auto& statuses_objects =
//       od_statuses_scans<trouble::registry, od::statuses_index>;
//
// The registry is that of `od_statuses`, and its rules hold.
template<auto& Registry, std::uint16_t Index>
  requires detail::od_known_registry<std::remove_cvref_t<decltype(Registry)>>
inline constexpr auto od_statuses_scans = [] {
  constexpr auto& live = detail::statuses_binding<Registry>::live;
  std::array<mpdo_scan, detail::od_status_runs(live)> scans{};
  auto run = 0uz;
  for (auto id = 0uz; id < live.size(); ++id) {
    if (!live[id]) continue;
    if (id == 0 || !live[id - 1]) {
      scans[run++] = {.index = Index,
                      .subindex = static_cast<std::uint8_t>(id + 1),
                      .count = 0};
    }
    ++scans[run - 1].count;
  }
  return scans;
}();

} // namespace canopen
} // namespace can
} // namespace emb
