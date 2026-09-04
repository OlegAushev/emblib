#pragma once

#include <emb/meta/fixed_string.hpp>
#include <emb/meta/typelist.hpp>
#include <emb/settings/param.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <type_traits>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace settings {

namespace detail {

// Never defined; see param.hpp.
void duplicate_parameter_name();
void parameter_id_collision();

template<fixed_string Name>
consteval auto unknown_parameter_message()
{
  return "Unknown parameter '" + Name + "'";
}

} // namespace detail

// The product's parameter list: descriptors in declaration order, plus the
// same indices ordered by identifier.
//
// Declaration order is meaningful — an index is the parameter's cell in the
// RAM image and its slot in a stored record — so the id order lives beside
// it rather than replacing it. Lookup by name is compile-time and linear;
// lookup by id happens once per cell while loading and is a binary search.
template<some_typelist Types, std::size_t N>
struct basic_schema {
  using types = Types;

  static constexpr std::size_t count = N;

  std::array<descriptor, N> parameters;
  std::array<std::uint16_t, N> by_id;

  consteval auto index_of(std::string_view name) const
      -> std::optional<std::size_t>
  {
    for (auto i = 0uz; i < N; ++i)
      if (parameters[i].name == name) return i;
    return std::nullopt;
  }

  constexpr auto find(std::uint32_t id) const -> std::optional<std::size_t>
  {
    auto const id_at = [this](std::uint16_t i) { return parameters[i].id; };
    auto const it = std::ranges::lower_bound(by_id, id, {}, id_at);
    if ((it == by_id.end()) || (id_at(*it) != id)) return std::nullopt;
    return *it;
  }
};

// make_schema(param(...), param(...), ...)
//
// Cross-parameter checks live here; per-parameter ones stay in param() so
// that they point at the offending line.
template<some_parameter_type... Ts>
consteval auto make_schema(declaration<Ts> const&... params)
    -> basic_schema<typelist<Ts...>, sizeof...(Ts)>
{
  constexpr auto n = sizeof...(Ts);
  basic_schema<typelist<Ts...>, n> schema{{params.desc...}, {}};

  for (auto i = 0uz; i < n; ++i)
    schema.by_id[i] = static_cast<std::uint16_t>(i);

  std::ranges::sort(schema.by_id, {}, [&schema](std::uint16_t i) {
    return schema.parameters[i].id;
  });

  // Two parameters may not share a name even when their types differ: the
  // compile-time lookup would resolve both to the first, and the image
  // would carry a cell nothing can reach.
  for (auto i = 0uz; i < n; ++i)
    for (auto j = i + 1; j < n; ++j)
      if (schema.parameters[i].name == schema.parameters[j].name) {
        detail::duplicate_parameter_name();
      }

  // Identifiers are what a stored record is matched by, so a hash collision
  // between two distinct names is as fatal as a duplicate name.
  for (auto i = 1uz; i < n; ++i)
    if (schema.parameters[schema.by_id[i]].id
        == schema.parameters[schema.by_id[i - 1]].id) {
      detail::parameter_id_collision();
    }

  return schema;
}

template<auto& Schema>
using schema_t = std::remove_cvref_t<decltype(Schema)>;

// The static type of the I-th parameter, recovered from the schema's
// typelist.
template<auto& Schema, std::size_t I>
using type_at = typelist_at_t<typename schema_t<Schema>::types, I>;

// The compile-time facts of one parameter, looked up by name:
// parameter<schema, "motor.p">::default_value.
//
// A nested type rather than a set of alias templates, for the reason the
// old registry documented: aliases are transparent to mangling, so spelling
// a value type through the lookup expression would embed the schema object
// and its whole typelist into every symbol that mentions it.
template<auto& Schema, fixed_string Name>
struct parameter {
  static constexpr auto lookup = Schema.index_of(Name.view());
  static_assert(lookup.has_value(), detail::unknown_parameter_message<Name>());

  // Falls back to the first parameter so that a wrong name produces the
  // static_assert alone: fed nothing, the accessors below would pile an
  // out-of-bounds error on top of it.
  static constexpr std::size_t index = lookup.value_or(0);

  static constexpr auto name = Name;
  using type = type_at<Schema, index>;

  static constexpr descriptor const& desc = Schema.parameters[index];
  static constexpr type default_value = from_raw<type>(desc.def);
};

} // namespace settings
} // namespace emb
