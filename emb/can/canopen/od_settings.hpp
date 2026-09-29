#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <variant>

#include "od.hpp"

#include <emb/meta/alternative_of.hpp>
#include <emb/meta/fixed_string.hpp>
#include <emb/settings/image.hpp>
#include <emb/settings/schema.hpp>
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

template<typename R>
concept settings_outcome = requires(R const& r) {
  { r.has_value() } -> std::same_as<bool>;
  { r.error() } -> std::convertible_to<settings::error>;
};

template<fixed_string Name>
consteval auto not_exposed_message()
{
  return "od: settings parameter '" + Name + "' is not exposed";
}

template<fixed_string Name>
consteval auto not_writable_message()
{
  return "od: settings parameter '"
       + Name
       + "' is not writable; bind it with ro";
}

} // namespace detail

// The concept `od_settings_accessors<GetAt, SetAt, RestoreAt>` is satisfied if
// and only if the three are the by-index accessors of a settings image: taking
// a parameter index, `GetAt` returns `std::expected<settings::value,
// settings::error>`, and `SetAt`, which also takes the value, and `RestoreAt`
// return `std::expected` of anything with `settings::error`.
template<auto GetAt, auto SetAt, auto RestoreAt>
concept od_settings_accessors =
    requires(std::size_t index, settings::value const& value) {
      {
        GetAt(index)
      } -> std::same_as<std::expected<settings::value, settings::error>>;
      { SetAt(index, value) } -> detail::settings_outcome;
      { RestoreAt(index) } -> detail::settings_outcome;
    };

// The class template `od_settings` binds objects to the parameters of the
// settings schema `Schema`, which it reaches through the application's
// by-index accessors, the ones that also record what a write changed. A row
// names its parameter once, as in `od_settings<...>::rw<"motor.p">`: the
// object's type is the parameter's, its `arg` is the parameter's index, and
// an `rw` object is restorable through 1011h:04 by `RestoreAt`. `ro` binds a
// parameter for reading only. Every row carries `catalog`, so
// `make_dictionary` checks that each exposed parameter has exactly one row
// and each hidden one none, as soon as the dictionary has one such row.
//
// Binding an unknown, hidden or, with `rw`, read-only parameter is reported
// at the row. A value of a type no parameter can hold is refused with
// `data_type_mismatch`; the accessors' errors map through `to_sdo_abort`.
template<auto& Schema, auto GetAt, auto SetAt, auto RestoreAt>
  requires od_settings_accessors<GetAt, SetAt, RestoreAt>
class od_settings {
  static constexpr std::size_t count = settings::schema_t<Schema>::count;
  static_assert(count <= 0xFFFF, "od: a parameter index must fit in arg");

  static constexpr std::array<std::string_view, count> names = [] {
    std::array<std::string_view, count> out{};
    for (auto i = 0uz; i < count; ++i) {
      out[i] = Schema.parameters[i].name;
    }
    return out;
  }();

  static constexpr std::array<bool, count> exposed = [] {
    std::array<bool, count> out{};
    for (auto i = 0uz; i < count; ++i) {
      out[i] = Schema.parameters[i].expose;
    }
    return out;
  }();

  template<fixed_string Name, od_access Access>
  struct binding {
    template<typename Ctx>
    consteval operator od_binding<Ctx>() const
    {
      using param = settings::parameter<Schema, Name>;
      constexpr bool known = param::lookup.has_value();
      constexpr bool shown = !known || param::desc.expose;
      constexpr bool allowed =
          !known || (Access != od_access::rw) || param::desc.writable;
      static_assert(shown, detail::not_exposed_message<Name>());
      static_assert(allowed, detail::not_writable_message<Name>());

      constexpr bool writes = (Access == od_access::rw);
      return {.access = Access,
              .type = od_type_of<settings::scalar_t<typename param::type>>,
              .read = &od_settings::read<Ctx>,
              .write = writes ? &od_settings::write<Ctx> : nullptr,
              .arg = static_cast<std::uint16_t>(param::index),
              .restore = writes ? &od_settings::restore<Ctx> : nullptr,
              .catalog = &catalog,
              .diagnosed = !(known && shown && allowed)};
    }
  };

public:
  static constexpr od_catalog catalog{"settings parameter", names, exposed};

  template<fixed_string Name>
  static constexpr binding<Name, od_access::rw> rw{};

  template<fixed_string Name>
  static constexpr binding<Name, od_access::ro> ro{};

  template<typename Ctx>
  static constexpr od_read_result read(Ctx&, std::uint16_t index)
  {
    auto const value = GetAt(index);
    if (!value) return std::unexpected(to_sdo_abort(value.error()));
    return value->visit([](auto v) { return od_value{v}; });
  }

  template<typename Ctx>
  static constexpr od_write_result write(Ctx&,
                                         std::uint16_t index,
                                         od_value value)
  {
    auto const setting = detail::to_settings_value(value);
    if (!setting) return std::unexpected(sdo_abort_code::data_type_mismatch);
    if (auto const written = SetAt(index, *setting); !written) {
      return std::unexpected(to_sdo_abort(written.error()));
    }
    return {};
  }

  template<typename Ctx>
  static constexpr od_write_result restore(Ctx&, std::uint16_t index)
  {
    if (auto const restored = RestoreAt(index); !restored) {
      return std::unexpected(to_sdo_abort(restored.error()));
    }
    return {};
  }
};

} // namespace canopen
} // namespace can
} // namespace emb
