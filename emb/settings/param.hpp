#pragma once

#include <emb/i18n.hpp>
#include <emb/meta/unit_symbol.hpp>
#include <emb/settings/value.hpp>

#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

#include <cstdint>

namespace emb {
namespace settings {

// The scoped enumeration `apply_policy` defines when a change to the value of
// a parameter may take effect. Whether the application can take a change in
// its present state is its own business, decided by its applier when the
// change is applied, not the schema's.
enum class apply_policy : std::uint8_t {
  // Policy of a parameter whose change takes effect when the application
  // applies it on command, `section::apply` and its kin; until then the
  // change stays pending.
  on_command,
  // Policy of a parameter that decides which objects exist or how a
  // peripheral is set up, or that is handed to an object only when the object
  // is built: a change takes effect only after a restart.
  on_restart,
};

// Type-erased description of one parameter, the same structure whatever the
// type of the parameter. `def`, `min` and `max` are cells encoded under
// `kind`: the default value and the bounds, inclusive, of the values the
// parameter accepts.
struct descriptor {
  std::string_view name;
  // Identifier that matches a stored cell to the parameter, unique within a
  // schema. It depends on `name` and `kind` only: renaming the parameter or
  // changing its scalar kind changes it, and a value stored before no longer
  // matches, so the parameter is loaded with its default. Changing its type
  // within the kind, e.g. its unit, keeps it, and the value stored before is
  // read as the new type: a parameter whose stored values would mean
  // something else is renamed.
  std::uint32_t id;
  scalar_kind kind;
  cell def;
  cell min;
  cell max;
  apply_policy apply;
  // Whether a transport may change the value. If `false`, `image::set_at`
  // and `image::restore_default_at` fail with `error::read_only`, while the
  // accessors by declaration still write it.
  bool writable;
  // Whether the parameter is meant to be reached by a transport at all. The
  // `image` does not consult it; a dictionary bound through
  // `can::canopen::od_settings` has a row for each parameter with `expose`
  // and none for the others.
  bool expose;

  // Two descriptors are equal if every field is; `name` compares as the
  // characters it views.
  friend constexpr bool operator==(descriptor const&,
                                   descriptor const&) = default;
};

namespace detail {

// The functions below fail the constant evaluation that calls them, and the
// diagnostic names the function: they are neither `constexpr` nor defined.
// `param` calls them to reject a declaration at its call: for a default
// outside the bounds, a lower bound above the upper one, an exposed
// parameter without a title in each language, a unit that only repeats the
// symbol of the type, and a unit given to a `bool`.
void parameter_default_outside_range();
void parameter_min_above_max();
void parameter_title_missing();
void parameter_unit_repeats_symbol();
void parameter_unit_meaningless();

// Returns `T` constructed from the lowest finite value of `scalar_t<T>`. The
// constructor of a wrapper may clamp it, e.g. that of `emb::clamped` to its
// lower limit.
template<some_parameter_type T>
constexpr T lowest()
{
  return T(std::numeric_limits<scalar_t<T>>::lowest());
}

// Returns `T` constructed from the highest finite value of `scalar_t<T>`. The
// constructor of a wrapper may clamp it, e.g. that of `emb::clamped` to its
// upper limit.
template<some_parameter_type T>
constexpr T highest()
{
  return T(std::numeric_limits<scalar_t<T>>::max());
}

// Returns the identifier of the parameter named `name` of the scalar kind
// `kind`: the 32-bit FNV-1a hash of the bytes of `name` followed by the byte
// `std::to_underlying(kind)`. Nothing else enters it, so a stored value still
// matches its parameter after a change of default, bounds, apply policy,
// flags, position in the schema or type within the kind, and no longer does
// after a change of name or kind. Records hold these identifiers: a change to
// the computation leaves every stored value unmatched.
constexpr std::uint32_t identify(std::string_view name, scalar_kind kind)
{
  std::uint32_t h = 0x811C9DC5u;
  auto const mix = [&h](std::uint8_t byte) {
    h ^= byte;
    h *= 0x01000193u;
  };
  for (char c : name) {
    mix(static_cast<std::uint8_t>(c));
  }
  mix(std::to_underlying(kind));
  return h;
}

} // namespace detail

// The class template `options` holds the optional part of the declaration of
// a parameter of type `T`, as `param` takes it. Each field from `min` on sets
// the field of the same name in the `descriptor` of the parameter; `title` and
// `unit` are kept in the `declaration`, outside the descriptor, which a
// schema keeps in the image.
template<some_parameter_type T>
struct options {
  // Name of the parameter for people, in each language. `param` rejects an
  // exposed parameter (`expose`) without a title in each: a transport shows
  // it to them.
  i18n title{};
  // Symbol of the unit of the value, for a type that names no unit or one that
  // does not describe the parameter, e.g. "rpm/s" for a slope held as an
  // `rpm_f32`: it replaces the symbol of the type
  // (`emb::unit_symbol_of<T>()`). `param` rejects a unit equal to that
  // symbol, which would only repeat it, and any unit of a `bool`.
  std::string_view unit{};
  // Lower and upper bound of the value, inclusive. If left out, each is the
  // widest `T` admits: the lowest or the highest finite value of
  // `scalar_t<T>`, converted to `T`. A wrapper that clamps, e.g.
  // `emb::clamped`, thus brings its own bounds, and a floating-point
  // parameter rejects infinities.
  T min = detail::lowest<T>();
  T max = detail::highest<T>();
  // Apply policy of the parameter, `apply_policy::on_restart` if left out: a
  // parameter whose policy was never chosen costs a restart rather than
  // taking effect on command.
  apply_policy apply = apply_policy::on_restart;
  bool writable = true;
  bool expose = true;
};

// The class template `declaration` holds a parameter as `param` declares it:
// its `descriptor`, its title and unit for people, and its type as `T`, which
// `type` names and from which `make_schema` builds the typelist of the
// schema. Kept in a `constexpr` variable with static storage duration, a
// declaration is also what names its parameter to `parameter`, `image` and
// `section`:
//
//   inline constexpr auto pole_pairs = param(
//       "motor.pole_pairs", std::int32_t{11}, {.title = {"Pole pairs", "…"}});
//   ...
//   values.get<pole_pairs>()
template<some_parameter_type T>
struct declaration {
  using type = T;

  descriptor desc;
  // Name of the parameter for people, as `options::title` gives it.
  i18n title;
  // Symbol of the unit of the value: `options::unit` if given, otherwise the
  // symbol of the type, empty if the type names none.
  std::string_view unit;
};

namespace detail {

template<typename T>
inline constexpr bool is_declaration = false;

template<typename T>
inline constexpr bool is_declaration<declaration<T>> = true;

} // namespace detail

// The concept `some_declaration<T>` is satisfied if and only if
// `std::remove_cvref_t<T>` is a specialization of `declaration`, as is
// `decltype(Decl)` for a template parameter `auto& Decl` that refers to a
// declaration. Templates that take a declaration state it as
// `template<auto& Decl> requires some_declaration<decltype(Decl)>` rather
// than as `some_declaration auto& Decl`, which Clang does not accept.
template<typename T>
concept some_declaration = detail::is_declaration<std::remove_cvref_t<T>>;

// Declares a parameter for `make_schema`: named `name`, with the default value
// `def` and the rest of the declaration in `opts`.
//
//   param("motor.pole_pairs", std::int32_t{11},
//         {.title = {"Pole pairs", "…"}, .min = 1, .max = 64, .apply = ...})
//
// The type of the parameter is that of `def`, so a literal must have that
// type exactly: `0.05f`, not `0.05`, and `std::int32_t{1}`, not `1`, an `int`,
// which on some targets is not `std::int32_t`. Compilation fails at the call
// if `opts.min` is above `opts.max`, if `def` lies outside
// [`opts.min`, `opts.max`], if the parameter is exposed without a title in
// each language, or if `opts.unit` repeats the symbol of `T` or is given to a
// `bool`.
template<some_parameter_type T>
consteval declaration<T> param(std::string_view name,
                               T def,
                               options<T> opts = {})
{
  constexpr scalar_kind kind = kind_of<T>;
  auto const min = to_cell(opts.min);
  auto const max = to_cell(opts.max);

  if (!less_equal(kind, min, max)) {
    detail::parameter_min_above_max();
  }
  if (!in_range(kind, to_cell(def), min, max)) {
    detail::parameter_default_outside_range();
  }
  if (opts.expose && (opts.title.en.empty() || opts.title.ru.empty())) {
    detail::parameter_title_missing();
  }
  if (!opts.unit.empty() && opts.unit == unit_symbol_of<T>()) {
    detail::parameter_unit_repeats_symbol();
  }
  if (!opts.unit.empty() && kind == scalar_kind::boolean) {
    detail::parameter_unit_meaningless();
  }

  return {descriptor{.name = name,
                     .id = detail::identify(name, kind),
                     .kind = kind,
                     .def = to_cell(def),
                     .min = min,
                     .max = max,
                     .apply = opts.apply,
                     .writable = opts.writable,
                     .expose = opts.expose},
          opts.title,
          opts.unit.empty() ? unit_symbol_of<T>() : opts.unit};
}

} // namespace settings
} // namespace emb
