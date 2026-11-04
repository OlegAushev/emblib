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
inline constexpr auto od_severity_scale = [] {
  constexpr auto info = emb::enum_info_of<Level>();
  std::array<od_value_info, info.size() + 1> out{};
  out[0] = {.value = severity_value_of<Level>(std::nullopt),
            .name = "none",
            .label = {"None", "Нет"}};
  for (auto i = 0uz; i < info.size(); ++i) {
    out[i + 1] = {.value = severity_value_of<Level>(info[i].value()),
                  .name = info[i].name(),
                  .label = info[i].label()};
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
// `S` states its name, `name`, and its label, `label`, which describe its
// member of the record of the statuses.
template<typename S>
concept od_labelled_status = requires {
  { S::name } -> std::convertible_to<std::string_view>;
  { S::label } -> std::convertible_to<i18n::text>;
};

// Whether each status of the list states its name and its label.
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
    constexpr auto info = emb::enum_info_of<Level>();
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

inline constexpr std::string_view od_unlabelled_levels =
    "od: the enumeration of the levels of the statuses must label each level "
    "of the registry, from 0, and no other value";

inline constexpr std::string_view od_too_many_statuses =
    "od: there must be at most 254 statuses: a status takes subindex id + 1, "
    "and the last is FEh";

inline constexpr std::string_view od_unlabelled_status =
    "od: each status must state its name and its label, which describe its "
    "member";

// The binding of the record of the statuses of `Registry` (`od_statuses`):
// its layout, a template of the context, and the info of its members.
template<auto& Registry>
struct statuses_binding {
  using traits = od_registry_traits<std::remove_cvref_t<decltype(Registry)>>;
  using status_list = typename traits::status_list;
  using level_type = typename traits::level_type;

  // A member for each status, at subindex id + 1.
  static constexpr std::size_t status_count = status_list::size;

  static_assert(od_levels_labelled<level_type, traits::level_count>(),
                od_unlabelled_levels);
  static_assert(status_count <= 0xFE, od_too_many_statuses);
  static_assert(od_statuses_labelled(status_list{}), od_unlabelled_status);

  // The subindex of the member of `S`.
  template<typename S>
  static constexpr std::size_t subindex_of =
      trouble::id_of<S, status_list>() + 1;

  // -- Members of the record, whose arg is not used --

  template<typename Ctx>
  static constexpr od_read_result read_highest_subindex(Ctx&, std::uint16_t)
  {
    return od_value{static_cast<std::uint8_t>(status_count)};
  }

  // Reads the severity of `Status`, the one call the bridge makes to the
  // registry.
  template<typename Ctx, typename Status>
  static constexpr od_read_result read_severity(Ctx&, std::uint16_t)
  {
    return od_value{severity_value_of(Registry.severity(Status{}))};
  }

  // The fields of the record: 00h, then a reader of the severity of each
  // status at subindex id + 1.
  template<typename Ctx>
  static constexpr auto layout = [] {
    std::array<od_field<Ctx>, status_count + 1> fields{};
    fields[0] = {.access = od_access::const_,
                 .type = od_data_type::uint8,
                 .read = &read_highest_subindex<Ctx>};
    trouble::for_each<status_list>([&]<typename S>(S) {
      fields[subindex_of<S>] = {.access = od_access::ro,
                                .type = od_data_type::uint8,
                                .read = &read_severity<Ctx, S>};
    });
    return fields;
  }();

  // The info of the members: that of 00h, then, at subindex id + 1, the name
  // and the label of each status; each member past 00h takes its values from
  // the scale.
  static constexpr auto field_info = [] {
    constexpr std::span<od_value_info const> scale =
        od_severity_scale<level_type>;
    std::array<od_field_info, status_count + 1> out{};
    out[0] = {.name = "highest_subindex",
              .label = {"Highest subindex", "Наибольший подындекс"}};
    trouble::for_each<status_list>([&]<typename S>(S) {
      if constexpr (od_labelled_status<S>) {
        out[subindex_of<S>] = {.name = S::name,
                               .label = S::label,
                               .value_table = scale};
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
            .field_info = field_info};
  }
};

} // namespace detail

// The variable template `od_statuses<Registry>` binds, in an `od_row`, the
// record of the statuses of the trouble registry `Registry`
// (`emb::trouble::registry`): the severity of each status, which a host finds
// by the path of the record and the name of the status,
// "trouble.statuses.ov_dc". The row names the record with subindex 00h and
// gives its info, and the area of its index, of the class `status`, tells a
// host what the record is:
//
//   {{0x4000, 0x00}, {"trouble.statuses", {"Trouble statuses", "Статусы неисправностей"}}, od_statuses<trouble::registry>},
//
// Its members, by subindex:
//
//   00h       const u8  the highest subindex: the number of statuses
//   id + 1    ro u8     the severity of the status of the id
//                       (`severity_value_of`): 0 if it stands at no level,
//                       and the level + 1 if it stands
//
// The id of a status is its position in the list (`trouble::id_of`), so the
// members follow the list without holes, and a status inserted or retired
// moves the members after it. A member of a status is named and labelled by
// the status's `name` and `label`, and takes its values from the scale of
// severity (`od_severity_scale`).
//
// The bridge calls only `severity` of the registry, a status at a time, and
// takes the statuses and the levels from the type of the registry
// (`detail::od_registry_traits`). The enumeration of the levels must label
// each of its levels from 0 and no other value, as the scale names the
// severities a member reads; there must be at most 254 statuses, as the last
// subindex is FEh; and each status must state its name and its label. A
// registry that breaks one fails to compile, at the row. Like the info of the rows, the
// names and the labels of the members exist at compile time only: the image
// holds the layout and a reader for each status.
template<auto& Registry>
  requires detail::od_known_registry<std::remove_cvref_t<decltype(Registry)>>
inline constexpr detail::statuses_binding<Registry> od_statuses{};

// The variable template `od_statuses_scans<Registry, Index>` is the list of the
// objects of the record of the statuses of `Registry` at `Index`
// (`od_statuses`) that an MPDO in source address mode sends: one block
// `{Index, 01h, n}` of its n statuses, and none if it has none. 00h is not in
// it, as it tells a host nothing and would take a frame of the round. An
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
  constexpr auto count = detail::statuses_binding<Registry>::status_count;
  std::array<mpdo_scan, (count > 0 ? 1 : 0)> scans{};
  if constexpr (count > 0) {
    scans[0] = {.index = Index,
                .subindex = 1,
                .count = static_cast<std::uint8_t>(count)};
  }
  return scans;
}();

} // namespace canopen
} // namespace can
} // namespace emb
