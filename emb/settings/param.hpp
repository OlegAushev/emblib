#pragma once

#include <emb/settings/value.hpp>

#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

#include <cstdint>

namespace emb {
namespace settings {

// The scoped enumeration `apply_policy` defines when a change to the value of
// a parameter may take effect. The enumerators are ordered from the least to
// the most demanding: wherever a change of one policy may take effect, so may
// a change of any policy before it. `pending_changes` relies on that order.
enum class apply_policy : std::uint8_t {
  // Policy of a parameter that feeds only state the application can
  // recompute at any time: a change may take effect at once, in any state.
  live,
  // Policy of a parameter whose change may take effect only in a state where
  // the application can reconfigure safely; until then the change stays
  // pending.
  on_safe_state,
  // Policy of a parameter that decides which objects exist or how a
  // peripheral is set up, or that is handed to an object only when the object
  // is built: a change takes effect only after a restart.
  on_restart,
};

// The class `group_id` identifies a group, i.e. the configuration a parameter
// feeds and the unit in which the application applies changes. The
// application names its groups with a scoped enumeration, which converts to
// `group_id` implicitly:
//
//   enum class group : std::uint8_t { drive, model, ... };
//   param("model.speed_Kp", 0.8f, {.group = group::model, ...})
//
// A group that `pending_changes` tracks must have a `value` less than
// `pending_changes::group_limit`.
struct group_id {
  std::uint8_t value = 0;

  constexpr group_id() = default;

  constexpr explicit group_id(std::uint8_t v) : value(v) {}

  // Constructs the identifier of the group `e`, holding the underlying value
  // of `e` as a `std::uint8_t`. Takes only a scoped enumeration whose
  // underlying type is one byte wide: a wider one fails to compile rather
  // than being truncated.
  template<typename E>
    requires std::is_scoped_enum_v<E>
          && (sizeof(std::underlying_type_t<E>) == 1)
  constexpr group_id(E e)
      : value(static_cast<std::uint8_t>(std::to_underlying(e)))
  {
  }

  friend constexpr bool operator==(group_id, group_id) = default;
};

// Report of a write that changed the value of a parameter: the group and the
// apply policy of the parameter, which `pending_changes::mark` records.
struct change {
  group_id group;
  apply_policy apply;

  friend constexpr bool operator==(change, change) = default;
};

// Type-erased description of one parameter, the same structure whatever the
// type of the parameter. `def`, `min` and `max` are cells encoded under
// `type`: the default value and the bounds, inclusive, of the values the
// parameter accepts.
struct descriptor {
  std::string_view name;
  // Identifier that matches a stored cell to the parameter, unique within a
  // schema. It depends on `name` and `type` only: renaming or retyping the
  // parameter changes it, and a value stored before no longer matches, so
  // the parameter is loaded with its default.
  std::uint32_t id;
  value_type type;
  raw_value def;
  raw_value min;
  raw_value max;
  group_id group;
  apply_policy apply;
  // Whether a transport may change the value. If `false`, `image::set_at`
  // and `image::restore_default_at` fail with `error::read_only`, while the
  // accessors by name still write it.
  bool writable;
  // Whether the parameter is meant to be reached by a transport at all. The
  // `image` does not consult it; a dictionary bound through
  // `can::canopen::od_settings` has a row for each parameter with `expose`
  // and none for the others.
  bool expose;
};

namespace detail {

// `parameter_default_outside_range` and `parameter_min_above_max` fail the
// constant evaluation that calls them, and the diagnostic names the
// function: they are neither `constexpr` nor defined. `param` calls them to
// reject a declaration at its call.
void parameter_default_outside_range();
void parameter_min_above_max();

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

// Returns the identifier of the parameter named `name` of type `type`: the
// 32-bit FNV-1a hash of the bytes of `name` followed by the byte
// `std::to_underlying(type)`. Nothing else enters it, so a stored value still
// matches its parameter after a change of default, bounds, group, apply
// policy, flags or position in the schema, and no longer does after a change
// of name or type. Records hold these identifiers: a change to the
// computation leaves every stored value unmatched.
constexpr std::uint32_t identify(std::string_view name, value_type type)
{
  std::uint32_t h = 0x811C9DC5u;
  auto const mix = [&h](std::uint8_t byte) {
    h ^= byte;
    h *= 0x01000193u;
  };
  for (char c : name) {
    mix(static_cast<std::uint8_t>(c));
  }
  mix(std::to_underlying(type));
  return h;
}

} // namespace detail

// The class template `options` holds the optional part of the declaration of
// a parameter of type `T`, as `param` takes it. Each field sets the field of
// the same name in the `descriptor` of the parameter.
template<some_parameter_type T>
struct options {
  // Lower and upper bound of the value, inclusive. If left out, each is the
  // widest `T` admits: the lowest or the highest finite value of
  // `scalar_t<T>`, converted to `T`. A wrapper that clamps, e.g.
  // `emb::clamped`, thus brings its own bounds, and a floating-point
  // parameter rejects infinities.
  T min = detail::lowest<T>();
  T max = detail::highest<T>();
  group_id group{};
  // Apply policy of the parameter, `apply_policy::on_restart` if left out: a
  // parameter whose policy was never chosen costs a restart rather than
  // taking effect at once.
  apply_policy apply = apply_policy::on_restart;
  bool writable = true;
  bool expose = true;
};

// The class template `declaration` holds a parameter as `param` declares it:
// its `descriptor`, and its type as `T`, from which `make_schema` builds the
// typelist of the schema.
template<some_parameter_type T>
struct declaration {
  descriptor desc;
};

// Declares a parameter for `make_schema`: named `name`, with the default value
// `def` and the rest of the declaration in `opts`.
//
//   param("motor.p", std::int32_t{11}, {.min = 1, .max = 64, .group = ...})
//
// The type of the parameter is that of `def`, so a literal must have that
// type exactly: `0.05f`, not `0.05`, and `std::int32_t{1}`, not `1`, an `int`,
// which on some targets is not `std::int32_t`. Compilation fails at the call
// if `opts.min` is above `opts.max` or `def` lies outside
// [`opts.min`, `opts.max`].
template<some_parameter_type T>
consteval declaration<T> param(std::string_view name,
                               T def,
                               options<T> opts = {})
{
  constexpr value_type type = type_of<T>;
  auto const min = to_raw(opts.min);
  auto const max = to_raw(opts.max);

  if (!less_equal(type, min, max)) {
    detail::parameter_min_above_max();
  }
  if (!in_range(type, to_raw(def), min, max)) {
    detail::parameter_default_outside_range();
  }

  return {descriptor{.name = name,
                     .id = detail::identify(name, type),
                     .type = type,
                     .def = to_raw(def),
                     .min = min,
                     .max = max,
                     .group = opts.group,
                     .apply = opts.apply,
                     .writable = opts.writable,
                     .expose = opts.expose}};
}

} // namespace settings
} // namespace emb
