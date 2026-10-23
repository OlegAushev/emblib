#pragma once

#include <emb/assert.hpp>
#include <emb/meta/fixed_string.hpp>
#include <emb/meta/typelist.hpp>
#include <emb/settings/param.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <optional>
#include <string_view>
#include <type_traits>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace settings {

namespace detail {

// The name that the declaration `Decl` gives its parameter, as a
// `fixed_string`, for the messages of failed `static_assert`s.
template<auto& Decl>
inline constexpr auto name_of = [] {
  fixed_string<Decl.desc.name.size() + 1> name;
  std::ranges::copy(Decl.desc.name, name.chars);
  return name;
}();

template<fixed_string Name>
consteval auto unlisted_parameter_message()
{
  return "Parameter '" + Name + "' is not in the schema";
}

template<fixed_string Name>
consteval auto redeclared_parameter_message()
{
  return "Parameter '" + Name + "' is declared otherwise in the schema";
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
// their order in the schema: the index of a parameter is its position among
// `params`. Templates such as `parameter` take a schema and a declaration by
// reference, as `auto& Schema` and `auto& Decl`, so the result and the
// declarations are kept in `constexpr` variables with static storage
// duration:
//
//   inline constexpr auto phase_swap =
//       param("drive.phase_swap", false, {.title = {"Phase swap", "…"}});
//   inline constexpr auto pole_pairs =
//       param("motor.pole_pairs",
//             std::int32_t{11},
//             {.title = {"Pole pairs", "…"}, .min = std::int32_t{1}});
//   inline constexpr auto schema = make_schema(phase_swap, pole_pairs);
//
// A parameter that is reached only by index, e.g. by a transport, may be
// declared in the call itself.
//
// The program is ill-formed if two parameters have the same name, even with
// different types, or if two parameters with different names have the same
// identifier, and the diagnostic says which.
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
        EMB_PANIC("two parameters have the same name");
      }

  // Identifiers are what a stored record is matched by, so a hash collision
  // between two distinct names is as fatal as a duplicate name.
  for (auto i = 1uz; i < n; ++i)
    if (schema.parameters[schema.by_id[i]].id
        == schema.parameters[schema.by_id[i - 1]].id) {
      EMB_PANIC("two parameters with different names have the same identifier");
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
// parameter that the declaration `Decl` declares in the schema `Schema`: its
// `name` as a `fixed_string`, its `index`, its `type` as `type_at` gives it,
// its descriptor `desc` and its `default_value` of type `type`, e.g.
// `parameter<schema, pole_pairs>::default_value`. The program is ill-formed
// unless `Schema` holds `Decl`, i.e. has a parameter of its name whose type
// and descriptor are those of `Decl`: the declaration `make_schema` was
// given, or one equal to it. The message of the failed `static_assert`
// contains the name.
//
// `parameter` is a class rather than a set of alias templates so that a
// signature naming `parameter<Schema, Decl>::type` mangles as that name: an
// alias would mangle as its expansion, which spells out the types of all
// parameters of `Schema`.
template<auto& Schema, auto& Decl>
  requires some_declaration<decltype(Decl)>
struct parameter {
  static constexpr auto name = detail::name_of<Decl>;

  static constexpr auto lookup = Schema.index_of(Decl.desc.name);
  static_assert(lookup.has_value(), detail::unlisted_parameter_message<name>());

  // Index of the parameter in `Schema`, or zero if `Schema` has no parameter
  // of the name of `Decl`. In that case `lookup` contains no value and
  // `type`, `desc` and `default_value` describe the first parameter, so that
  // the failed `static_assert` is the only error `parameter` causes.
  static constexpr std::size_t index = lookup.value_or(0);

  using type = type_at<Schema, index>;

  static constexpr descriptor const& desc = Schema.parameters[index];
  static_assert(
      !lookup.has_value()
          || (std::same_as<type,
                           typename std::remove_cvref_t<decltype(Decl)>::type>
              && desc == Decl.desc),
      detail::redeclared_parameter_message<name>());

  static constexpr type default_value = from_cell<type>(desc.def);
};

} // namespace settings
} // namespace emb
