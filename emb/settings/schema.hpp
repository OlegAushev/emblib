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

// `duplicate_parameter_name` and `parameter_id_collision` fail the constant
// evaluation that calls them, and the diagnostic names the function: they are
// neither `constexpr` nor defined. `make_schema` calls the first if two
// parameters have the same name and the second if two parameters with
// different names have the same identifier.
void duplicate_parameter_name();
void parameter_id_collision();

template<fixed_string Name>
consteval auto unknown_parameter_message()
{
  return "Unknown parameter '" + Name + "'";
}

} // namespace detail

// The class template `basic_schema` holds a schema, i.e. the list of
// parameters that `make_schema` returns: the descriptors of the `N`
// parameters in `parameters` and their types in the typelist `Types`, both in
// declaration order.
template<some_typelist Types, std::size_t N>
struct basic_schema {
  using types = Types;

  static constexpr std::size_t count = N;

  // Descriptors of the parameters, in declaration order. The position of a
  // parameter here is its index, which also locates its type in `types` and
  // its cell in an image.
  std::array<descriptor, N> parameters;
  // Indices into `parameters` in ascending order of identifier, i.e. a
  // permutation of [0, `N`) such that `parameters[by_id[i]].id` increases
  // with `i`. `make_schema` establishes this order; `find` relies on it.
  std::array<std::uint16_t, N> by_id;

  // Returns the index of the parameter named `name`, or `std::nullopt` if
  // there is none. Linear in `N`; callable only at compile time.
  consteval std::optional<std::size_t> index_of(std::string_view name) const
  {
    for (auto i = 0uz; i < N; ++i)
      if (parameters[i].name == name) return i;
    return std::nullopt;
  }

  // Returns the index of the parameter whose identifier is `id`, or
  // `std::nullopt` if there is none. Logarithmic in `N`: a binary search over
  // `by_id`. The behavior is undefined unless `by_id` is in the order
  // `make_schema` gives it.
  constexpr std::optional<std::size_t> find(std::uint32_t id) const
  {
    auto const id_at = [this](std::uint16_t i) { return parameters[i].id; };
    auto const it = std::ranges::lower_bound(by_id, id, {}, id_at);
    if ((it == by_id.end()) || (id_at(*it) != id)) return std::nullopt;
    return *it;
  }
};

// Returns the schema of the parameters `params`, in the order given, which is
// their declaration order. Templates such as `parameter` take a schema by
// reference, as `auto& Schema`, so the result is kept in a `constexpr`
// variable with static storage duration:
//
//   inline constexpr auto schema = make_schema(
//       param("drive.phase_swap", false),
//       param("motor.p", std::int32_t{11}, {.min = std::int32_t{1}}));
//
// The program is ill-formed if two parameters have the same name, even with
// different types, or if two parameters with different names have the same
// identifier; the diagnostic names `detail::duplicate_parameter_name` or
// `detail::parameter_id_collision`, respectively.
template<some_parameter_type... Ts>
consteval basic_schema<typelist<Ts...>, sizeof...(Ts)>
make_schema(declaration<Ts> const&... params)
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

// Type of the schema `Schema` without cv-qualifiers, a specialization of
// `basic_schema`.
template<auto& Schema>
using schema_t = std::remove_cvref_t<decltype(Schema)>;

// Type of the parameter at index `I` of the schema `Schema`, i.e. the type of
// the default passed to `param`, e.g. `units::rpm_f32` rather than its scalar
// `float`. The program is ill-formed if `I` is not less than
// `schema_t<Schema>::count`.
template<auto& Schema, std::size_t I>
using type_at = typelist_at_t<typename schema_t<Schema>::types, I>;

// The class template `parameter` provides the compile-time facts of the
// parameter named `Name` in the schema `Schema`: its `index`, its `type` as
// `type_at` gives it, its descriptor `desc` and its `default_value` of type
// `type`, e.g. `parameter<schema, "motor.p">::default_value`. The program is
// ill-formed if `Schema` has no parameter named `Name`; the message of the
// failed `static_assert` contains `Name`.
//
// `parameter` is a class rather than a set of alias templates so that a
// signature naming `parameter<Schema, Name>::type` mangles as that name: an
// alias would mangle as its expansion, which spells out the types of all
// parameters of `Schema`.
template<auto& Schema, fixed_string Name>
struct parameter {
  static constexpr auto lookup = Schema.index_of(Name.view());
  static_assert(lookup.has_value(), detail::unknown_parameter_message<Name>());

  // Index of the parameter in `Schema`, or zero if `Schema` has no parameter
  // named `Name`. In that case `lookup` contains no value and `type`, `desc`
  // and `default_value` describe the first parameter, so that the failed
  // `static_assert` is the only error `parameter` causes.
  static constexpr std::size_t index = lookup.value_or(0);

  static constexpr auto name = Name;
  using type = type_at<Schema, index>;

  static constexpr descriptor const& desc = Schema.parameters[index];
  static constexpr type default_value = from_raw<type>(desc.def);
};

} // namespace settings
} // namespace emb
