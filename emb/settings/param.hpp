#pragma once

#include <emb/i18n.hpp>
#include <emb/meta/enum_info.hpp>
#include <emb/meta/unit_symbol.hpp>
#include <emb/panic.hpp>
#include <emb/settings/value.hpp>

#include <limits>
#include <span>
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

// Returns the info of the enumerators of `apply_policy`, by which a transport
// names the policy of a parameter for people.
consteval std::span<enumerator_info<apply_policy> const> enum_info(apply_policy)
{
  static constexpr enumerator_info<apply_policy> info[] = {
      {apply_policy::on_command, "on_command", {"On command", "По команде"}},
      {apply_policy::on_restart,
       "on_restart",
       {"On restart", "После перезапуска"}},
  };
  return info;
}

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

// Returns `T` constructed from the lowest finite value of `scalar_t<T>`. The
// constructor of a wrapper may clamp it, e.g. that of `emb::clamped` to its
// lower limit. For an enumeration, returns the lowest value it labels.
template<some_parameter_type T>
constexpr T lowest()
{
  if constexpr (some_enum_value<T>) {
    return enum_info_of<T>().front().value();
  }
  else {
    return T(std::numeric_limits<scalar_t<T>>::lowest());
  }
}

// Returns `T` constructed from the highest finite value of `scalar_t<T>`. The
// constructor of a wrapper may clamp it, e.g. that of `emb::clamped` to its
// upper limit. For an enumeration, returns the highest value it labels.
template<some_parameter_type T>
constexpr T highest()
{
  if constexpr (some_enum_value<T>) {
    return enum_info_of<T>().back().value();
  }
  else {
    return T(std::numeric_limits<scalar_t<T>>::max());
  }
}

// Whether the enumeration `T` labels every value from `lo` to `hi`, as it
// must between the bounds of a parameter: a write is checked against the
// bounds only, so a value between them that `T` does not label would pass.
template<some_enum_value T>
consteval bool labelled_between(T lo, T hi)
{
  auto const low = static_cast<std::int64_t>(std::to_underlying(lo));
  auto const high = static_cast<std::int64_t>(std::to_underlying(hi));
  std::int64_t named = 0;
  for (auto const& enumerator : enum_info_of<T>()) {
    auto const value =
        static_cast<std::int64_t>(std::to_underlying(enumerator.value()));
    if (low <= value && value <= high) ++named;
  }
  return named == high - low + 1;
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
// the field of the same name in the `descriptor` of the parameter; `label` and
// `unit` are kept in the `declaration`, outside the descriptor, which a
// schema keeps in the image.
template<some_parameter_type T>
struct options {
  // Label of the parameter for people, in each language. `param` rejects an
  // exposed parameter (`expose`) without a label in each: a transport shows
  // it to them.
  i18n::text label{};
  // Symbol of the unit of the value, in each language, for a type that names
  // no unit or one that does not describe the parameter, e.g. "rpm/s" and
  // "об/(мин·с)" for a slope held as an `rpm_f32`: it replaces the symbol of
  // the type (`emb::unit_symbol_of<T>()`). `param` rejects a unit equal to
  // that symbol, which would only repeat it, any unit of a `bool` or of an
  // enumeration, whose values are named instead, and a unit without a symbol
  // in each language: a transport shows it with the value.
  i18n::text unit{};
  // Lower and upper bound of the value, inclusive. If left out, each is the
  // widest `T` admits: the lowest or the highest finite value of
  // `scalar_t<T>`, converted to `T`, and for an enumeration the lowest or the
  // highest value it labels. A wrapper that clamps, e.g. `emb::clamped`, thus
  // brings its own bounds, and a floating-point parameter rejects infinities.
  // An enumeration must label every value between the bounds.
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
// its `descriptor`, its label and unit for people, and its type as `T`, which
// `type` names and from which `make_schema` builds the typelist of the
// schema. Kept in a `constexpr` variable with static storage duration, a
// declaration is also what names its parameter to `parameter`, `image` and
// `section`:
//
//   inline constexpr auto pole_pairs = param(
//       "motor.pole_pairs", std::int32_t{11}, {.label = {"Pole pairs", "…"}});
//   ...
//   values.get<pole_pairs>()
template<some_parameter_type T>
struct declaration {
  using type = T;

  descriptor desc;
  // Label of the parameter for people, as `options::label` gives it.
  i18n::text label;
  // Symbol of the unit of the value, in each language: `options::unit` if
  // given, otherwise the symbol of the type, `i18n::text{}` if the type names
  // none.
  i18n::text unit;
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
//         {.label = {"Pole pairs", "…"}, .min = 1, .max = 64, .apply = ...})
//
// The type of the parameter is that of `def`, so a literal must have that
// type exactly: `0.05f`, not `0.05`, and `std::int32_t{1}`, not `1`, an `int`,
// which on some targets is not `std::int32_t`. A parameter of an enumeration,
// e.g. `param("model.pwm_mode", pwm_mode::svpwm, {...})`, is stored as its
// carrier (`scalar_t`) and its identifier is that of a parameter of the
// carrier of the same name. Compilation fails at the call if `opts.min` is
// above `opts.max`, if `def` lies outside [`opts.min`, `opts.max`], if the
// parameter is exposed without a label in each language, if `opts.unit`
// repeats the symbol of `T` or is given to a `bool` or to an enumeration, if
// the unit, `opts.unit` or the symbol of `T`, lacks a symbol in a language,
// or if an enumeration does not label every value from `opts.min` to
// `opts.max`.
template<some_parameter_type T>
consteval declaration<T> param(std::string_view name,
                               T def,
                               options<T> opts = {})
{
  constexpr scalar_kind kind = kind_of<T>;
  auto const min = to_cell(opts.min);
  auto const max = to_cell(opts.max);

  if (!less_equal(kind, min, max)) {
    EMB_PANIC("the lower bound of a parameter is above the upper one");
  }
  if (!in_range(kind, to_cell(def), min, max)) {
    EMB_PANIC("the default of a parameter is outside its bounds");
  }
  if constexpr (some_enum_value<T>) {
    if (!detail::labelled_between(opts.min, opts.max)) {
      EMB_PANIC("the enumeration does not label a value between the bounds");
    }
  }
  if (opts.expose && !i18n::is_complete(opts.label)) {
    EMB_PANIC("an exposed parameter needs a label in each language");
  }
  if (opts.unit != i18n::text{} && opts.unit == unit_symbol_of<T>()) {
    EMB_PANIC("the unit only repeats the symbol of the type");
  }
  if (opts.unit != i18n::text{}
      && (kind == scalar_kind::boolean || some_enum_value<T>)) {
    EMB_PANIC("a bool or an enumeration takes no unit");
  }
  auto const unit = opts.unit == i18n::text{} ? unit_symbol_of<T>() : opts.unit;
  if (unit != i18n::text{} && !i18n::is_complete(unit)) {
    EMB_PANIC("the unit needs a symbol in each language");
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
          opts.label,
          unit};
}

} // namespace settings
} // namespace emb
