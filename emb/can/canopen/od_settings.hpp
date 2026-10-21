#pragma once

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
#include "od_handlers.hpp"

#include <emb/meta/alternative_of.hpp>
#include <emb/meta/fixed_string.hpp>
#include <emb/meta/typelist.hpp>
#include <emb/meta/unit_symbol.hpp>
#include <emb/settings/image.hpp>
#include <emb/settings/schema.hpp>
#include <emb/settings/section.hpp>
#include <emb/settings/value.hpp>

namespace emb {
namespace can {
namespace canopen {

// Returns the abort code that answers a settings access error.
constexpr sdo_abort_code to_sdo_abort(settings::error error)
{
  switch (error) {
  case settings::error::unknown_parameter:
    return sdo_abort_code::object_not_found;
  case settings::error::read_only: return sdo_abort_code::write_to_read_only;
  case settings::error::type_mismatch:
    return sdo_abort_code::data_type_mismatch;
  case settings::error::out_of_range:
    return sdo_abort_code::value_range_exceeded;
  }
  return sdo_abort_code::general_error;
}

// Returns the abort code that answers a refusal to apply settings: a
// parameter that nothing applies on command is not accessed so, a check that
// fails is a parameter incompatibility, and a change the application does not
// admit now waits for another device state.
constexpr sdo_abort_code to_sdo_abort(settings::apply_refusal refusal)
{
  switch (refusal) {
  case settings::apply_refusal::unknown_parameter:
    return sdo_abort_code::object_not_found;
  case settings::apply_refusal::not_applicable:
    return sdo_abort_code::unsupported_access;
  case settings::apply_refusal::invalid:
    return sdo_abort_code::parameter_incompatibility;
  case settings::apply_refusal::not_admitted:
    return sdo_abort_code::state_error;
  }
  return sdo_abort_code::general_error;
}

// Returns the abort code that answers a refusal to save settings: values
// that wait to be applied are a device state, values that fail their check
// a parameter incompatibility, and a failing medium a store error.
constexpr sdo_abort_code to_sdo_abort(settings::save_refusal refusal)
{
  switch (refusal) {
  case settings::save_refusal::unapplied: return sdo_abort_code::state_error;
  case settings::save_refusal::invalid:
    return sdo_abort_code::parameter_incompatibility;
  case settings::save_refusal::medium: return sdo_abort_code::data_store_error;
  }
  return sdo_abort_code::general_error;
}

namespace detail {

// Returns the value `value` holds as a settings value, or nullopt if no
// parameter can hold its alternative, e.g. a narrow integer.
constexpr std::optional<settings::value>
to_settings_value(od_value const& value)
{
  return value.visit([](auto v) -> std::optional<settings::value> {
    if constexpr (alternative_of<decltype(v), settings::value>) {
      return settings::value{v};
    }
    else {
      return std::nullopt;
    }
  });
}

template<fixed_string Name>
consteval auto not_exposed_message()
{
  return "od: settings parameter '" + Name + "' is not exposed";
}

template<fixed_string Name>
consteval auto actual_type_message()
{
  return "od: the actual value of settings parameter '"
       + Name
       + "' is not read as its type";
}

template<fixed_string Name>
consteval auto actual_unit_message()
{
  return "od: the actual value of settings parameter '"
       + Name
       + "' is read in another unit";
}

// Whether the reader `Actual` reads the actual value of a parameter of type
// `T` and unit `unit` in that unit: the type of the value it returns names no
// unit, or the symbol of `T`, or `unit`. A reader of `rpm_f32` fits a
// parameter of type `rpm_f32` whose unit is "rpm/s", one of `hz_f32` a
// parameter of type `float` whose unit is "Hz"; one of `rpm_f32` fits
// neither the latter nor a parameter of type `eradps_f32`.
template<auto Actual, typename T>
consteval bool od_reads_in_unit(std::string_view unit)
{
  using value_type = typename od_read_value<od_result_t<Actual>>::type;
  constexpr auto symbol = unit_symbol_of<value_type>();
  return symbol.empty() || symbol == unit_symbol_of<T>() || symbol == unit;
}

} // namespace detail

// The concept `od_settings_access<SectionOf, MakeApplier>` is satisfied if
// and only if `SectionOf` returns, from a reference to a context, a
// reference to a `settings::section`, and `MakeApplier`, from a reference to
// the same context, an applier of that section's schema.
template<auto SectionOf, auto MakeApplier>
concept od_settings_access =
    detail::od_context_function<SectionOf>
    && detail::od_context_function<MakeApplier>
    && std::same_as<detail::od_context_of<SectionOf>,
                    detail::od_context_of<MakeApplier>>
    && std::is_lvalue_reference_v<detail::od_result_t<SectionOf>>
    && settings::some_applier<
        detail::od_result_t<MakeApplier>,
        std::remove_reference_t<detail::od_result_t<SectionOf>>::schema>;

// The class template `od_settings` binds records of a dictionary to the
// parameters of a `settings::section`, and serves commands that apply and
// discard them all. It reaches the section through `SectionOf` and an applier
// through `MakeApplier`, both called with the context of the dictionary, so a
// section and an applier may live wherever the context finds them.
//
// `param<Decl>` binds the record of the parameter that `Decl` declares, and
// `param<Decl, Actual>` one that also shows its actual value, as the reader
// `Actual` reads it in the type and the unit of the parameter. The record's
// path is the parameter's name and its title the parameter's, so its row has
// no label of its own: `{key, {}, param<Decl>}`. The record's type and unit
// are the parameter's; its members, by subindex:
//
//   00h  const u8   the last subindex, 09h, or 0Ah with `Actual`
//   01h  rw         the value requested; ro if the parameter is not writable
//   02h  ro         the value applied
//   03h  ro         the value stored, which the next startup loads
//   04h  const      the default
//   05h  const      the lower bound
//   06h  const      the upper bound
//   07h  const u8   the apply policy: 0 on command, 1 on restart
//   08h  wo u32     applies the parameter, as `section::apply` does
//   09h  wo u32     discards its value requested, as `section::discard` does
//   0Ah  ro         the actual value, with `Actual` only
//
// `apply_all` and `discard_all` are commands for `od_exec`, `last_refusal` a
// reader for `od_ro`: the abort that answered the last refusal of an
// application or of a save, zero if there was none. A refusal answers a
// command with the abort of `to_sdo_abort`.
//
// A record of a parameter carries `catalog`, so `make_dictionary` checks that
// each exposed parameter has exactly one record and each hidden one none.
// Binding a declaration that the schema does not hold, a hidden parameter, or
// an `Actual` of another type or unit (`detail::od_reads_in_unit`) is
// reported at the row. A value of a type no
// parameter can hold is refused with `data_type_mismatch`.
template<auto SectionOf, auto MakeApplier>
  requires od_settings_access<SectionOf, MakeApplier>
class od_settings {
  using context_type = detail::od_context_of<SectionOf>;
  using section_type = std::remove_reference_t<detail::od_result_t<SectionOf>>;
  using field = od_field<context_type>;

  static constexpr auto& schema = section_type::schema;
  static constexpr std::size_t count = settings::schema_t<schema>::count;
  static_assert(count <= 0xFFFF, "od: a parameter index must fit in arg");

  static constexpr std::array<std::string_view, count> names = [] {
    std::array<std::string_view, count> out{};
    for (auto i = 0uz; i < count; ++i) {
      out[i] = schema.parameters[i].name;
    }
    return out;
  }();

  static constexpr std::array<bool, count> exposed = [] {
    std::array<bool, count> out{};
    for (auto i = 0uz; i < count; ++i) {
      out[i] = schema.parameters[i].expose;
    }
    return out;
  }();

  static constexpr od_read_result
  as_od(std::expected<settings::value, settings::error> const& value)
  {
    if (!value) return std::unexpected(to_sdo_abort(value.error()));
    return value->visit([](auto v) { return od_value{v}; });
  }

  static constexpr settings::descriptor const& desc(std::uint16_t index)
  {
    return schema.parameters[index];
  }

  // -- Members of the record of a parameter, whose arg is its index --

  template<std::uint8_t Last>
  static constexpr od_read_result last_subindex(context_type&, std::uint16_t)
  {
    return od_value{Last};
  }

  static constexpr od_read_result read_value(context_type& ctx,
                                             std::uint16_t index)
  {
    return as_od(SectionOf(ctx).get_at(index));
  }

  static constexpr od_write_result write_value(context_type& ctx,
                                               std::uint16_t index,
                                               od_value value)
  {
    auto const setting = detail::to_settings_value(value);
    if (!setting) return std::unexpected(sdo_abort_code::data_type_mismatch);
    if (auto const written = SectionOf(ctx).set_at(index, *setting); !written) {
      return std::unexpected(to_sdo_abort(written.error()));
    }
    return {};
  }

  static constexpr od_read_result read_applied(context_type& ctx,
                                               std::uint16_t index)
  {
    return as_od(SectionOf(ctx).applied_at(index));
  }

  static constexpr od_read_result read_stored(context_type& ctx,
                                              std::uint16_t index)
  {
    return as_od(SectionOf(ctx).stored_at(index));
  }

  static constexpr od_read_result read_default(context_type&,
                                               std::uint16_t index)
  {
    return as_od(settings::to_value(desc(index).kind, desc(index).def));
  }

  static constexpr od_read_result read_min(context_type&, std::uint16_t index)
  {
    return as_od(settings::to_value(desc(index).kind, desc(index).min));
  }

  static constexpr od_read_result read_max(context_type&, std::uint16_t index)
  {
    return as_od(settings::to_value(desc(index).kind, desc(index).max));
  }

  static constexpr od_read_result read_policy(context_type&,
                                              std::uint16_t index)
  {
    return od_value{std::to_underlying(desc(index).apply)};
  }

  static constexpr od_write_result apply_parameter(context_type& ctx,
                                                   std::uint16_t index,
                                                   od_value)
  {
    auto applier = MakeApplier(ctx);
    if (auto const applied = SectionOf(ctx).apply(index, applier); !applied) {
      return std::unexpected(to_sdo_abort(applied.error()));
    }
    return {};
  }

  static constexpr od_write_result discard_parameter(context_type& ctx,
                                                     std::uint16_t index,
                                                     od_value)
  {
    if (auto const discarded = SectionOf(ctx).discard(index); !discarded) {
      return std::unexpected(to_sdo_abort(discarded.error()));
    }
    return {};
  }

  // The members of the record of a parameter of type `Type`, with 01h
  // writable if `Writable` and with 0Ah read by `Actual` unless it is
  // nullptr.
  template<od_data_type Type, bool Writable, auto Actual>
  static constexpr auto param_layout = [] {
    constexpr bool actual = !std::is_null_pointer_v<decltype(Actual)>;
    std::array<field, actual ? 11 : 10> fields{{
        {.access = od_access::const_,
         .type = od_data_type::uint8,
         .read = &last_subindex < actual ? 0x0A : 0x09 > },
        {.access = Writable ? od_access::rw : od_access::ro,
         .type = Type,
         .read = &read_value,
         .write = Writable ? &write_value : nullptr},
        {.access = od_access::ro, .type = Type, .read = &read_applied},
        {.access = od_access::ro, .type = Type, .read = &read_stored},
        {.access = od_access::const_, .type = Type, .read = &read_default},
        {.access = od_access::const_, .type = Type, .read = &read_min},
        {.access = od_access::const_, .type = Type, .read = &read_max},
        {.access = od_access::const_,
         .type = od_data_type::uint8,
         .read = &read_policy},
        {.access = od_access::wo,
         .type = od_data_type::uint32,
         .write = &apply_parameter},
        {.access = od_access::wo,
         .type = od_data_type::uint32,
         .write = &discard_parameter},
    }};
    if constexpr (actual) {
      fields[0x0A] = {.access = od_access::ro,
                      .type = Type,
                      .read = &detail::od_read_thunk<Actual, context_type>};
    }
    return fields;
  }();

  // The labels of the members of the record of a parameter, for a host; a
  // record without `Actual` takes the first ten. The members that show a
  // value of the parameter inherit the record's type and unit.
  static constexpr od_field_label param_labels[] = {
      {.name = "highest_subindex",
       .title = {"Highest subindex", "Наибольший подындекс"}},
      {.name = "requested",
       .title = {"Requested value", "Запрошенное значение"},
       .inherits = true},
      {.name = "applied",
       .title = {"Applied value", "Применённое значение"},
       .inherits = true},
      {.name = "stored",
       .title = {"Stored value", "Сохранённое значение"},
       .inherits = true},
      {.name = "default",
       .title = {"Default value", "Значение по умолчанию"},
       .inherits = true},
      {.name = "min",
       .title = {"Lower bound", "Нижняя граница"},
       .inherits = true},
      {.name = "max",
       .title = {"Upper bound", "Верхняя граница"},
       .inherits = true},
      {.name = "policy", .title = {"Apply policy", "Политика применения"}},
      {.name = "apply", .title = {"Apply", "Применить"}, .command = true},
      {.name = "discard", .title = {"Discard", "Отбросить"}, .command = true},
      {.name = "actual",
       .title = {"Actual value", "Фактическое значение"},
       .inherits = true},
  };

  // Whether `Actual` is nullptr, or a reader whose value travels as the
  // scalar `type` names.
  template<auto Actual>
  static consteval bool reads_as(od_data_type type)
  {
    if constexpr (std::is_null_pointer_v<decltype(Actual)>) {
      return true;
    }
    else if constexpr (od_reader<Actual, context_type>) {
      using value_type =
          typename detail::od_read_value<detail::od_result_t<Actual>>::type;
      return od_type_of<unwrapped_t<value_type>> == type;
    }
    else {
      return false;
    }
  }

  // Whether `Actual` is nullptr, or a reader that reads the actual value of the
  // parameter `Decl` declares in its unit (`detail::od_reads_in_unit`).
  template<auto& Decl, auto Actual>
  static consteval bool reads_in_unit_of()
  {
    if constexpr (std::is_null_pointer_v<decltype(Actual)>) {
      return true;
    }
    else {
      using type = typename std::remove_cvref_t<decltype(Decl)>::type;
      return detail::od_reads_in_unit<Actual, type>(Decl.unit);
    }
  }

  template<auto& Decl, auto Actual>
  struct param_binding {
    consteval operator od_binding<context_type>() const
    {
      using param = settings::parameter<schema, Decl>;
      constexpr bool known = param::lookup.has_value();
      constexpr bool shown = !known || param::desc.expose;
      constexpr auto type =
          od_type_of<settings::scalar_t<typename param::type>>;
      constexpr bool fits = reads_as<Actual>(type);
      constexpr bool in_unit = !fits || reads_in_unit_of<Decl, Actual>();
      constexpr bool actual = !std::is_null_pointer_v<decltype(Actual)>;
      static_assert(shown, detail::not_exposed_message<param::name>());
      static_assert(fits, detail::actual_type_message<param::name>());
      static_assert(in_unit, detail::actual_unit_message<param::name>());

      return {.access = od_access::ro,
              .type = type,
              .arg = static_cast<std::uint16_t>(param::index),
              .catalog = &catalog,
              .layout = param_layout<type, param::desc.writable, Actual>,
              .field_labels =
                  std::span<od_field_label const>{param_labels}.first(
                      actual ? 11 : 10),
              .label = {.path = Decl.desc.name, .title = Decl.title},
              .unit = Decl.unit,
              .diagnosed = !(known && shown && fits && in_unit)};
    }
  };

public:
  static constexpr od_catalog catalog{"settings parameter", names, exposed};

  template<auto& Decl, auto Actual = nullptr>
    requires settings::some_declaration<decltype(Decl)>
  static constexpr param_binding<Decl, Actual> param{};

  static constexpr od_write_result apply_all(context_type& ctx)
  {
    auto applier = MakeApplier(ctx);
    if (auto const applied = SectionOf(ctx).apply_all(applier); !applied) {
      return std::unexpected(to_sdo_abort(applied.error()));
    }
    return {};
  }

  static constexpr void discard_all(context_type& ctx)
  {
    SectionOf(ctx).discard_all();
  }

  static constexpr std::uint32_t last_refusal(context_type& ctx)
  {
    auto const refused = SectionOf(ctx).last_refusal();
    if (!refused) return 0;
    return std::to_underlying(
        refused->visit([](auto reason) { return to_sdo_abort(reason); }));
  }
};

} // namespace canopen
} // namespace can
} // namespace emb
