#pragma once

#include <emb/settings/value.hpp>

#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

#include <cstdint>

namespace emb {
namespace settings {

// When a changed parameter may take effect. Declared per parameter, because
// only the schema knows which of them merely feed a computation and which
// decide what objects exist at all.
//
// The default is on_restart: opting into live application is a claim about
// the consuming code, and a parameter whose policy was never considered
// should cost a restart rather than silently take effect halfway through a
// control cycle.
enum class apply_policy : std::uint8_t {
  live,
  on_safe_state,
  on_restart,
};

// Which configuration a parameter feeds — the unit in which changes are
// applied. The set of groups belongs to the product, so this is an opaque
// number here, constructible from the application's own enum:
//
//   enum class group : std::uint8_t { drive, model, ... };
//   param("model.speed_Kp", 0.8f, {.group = group::model, ...})
//
// A one-byte underlying type is required rather than truncated to, so an
// enum that does not fit is a compile error instead of a collision.
struct group_id {
  std::uint8_t value = 0;

  constexpr group_id() = default;

  constexpr explicit group_id(std::uint8_t v) : value(v) {}

  template<typename E>
    requires std::is_scoped_enum_v<E>
          && (sizeof(std::underlying_type_t<E>) == 1)
  constexpr group_id(E e)
      : value(static_cast<std::uint8_t>(std::to_underlying(e)))
  {
  }

  friend constexpr bool operator==(group_id, group_id) = default;
};

// What a successful write changed. Handed back rather than acted upon: the
// image is data, and which task may apply a group and when is the
// application's decision — see pending_changes.
struct change {
  group_id group;
  apply_policy apply;

  friend constexpr bool operator==(change, change) = default;
};

// One parameter as the runtime sees it: everything type-erased into cells,
// so a table of these is uniform and a transport can walk it without
// knowing any parameter's static type.
struct descriptor {
  std::string_view name;
  std::uint32_t id;
  value_type type;
  raw_value def;
  raw_value min;
  raw_value max;
  group_id group;
  apply_policy apply;
  bool writable;
  bool expose;
};

namespace detail {

// Never defined: calling one from a constant expression fails the
// evaluation with this name in the diagnostic, at the offending param()
// call rather than somewhere down the instantiation chain.
void parameter_default_outside_range();
void parameter_min_above_max();

template<some_parameter_type T>
constexpr auto lowest() -> T
{
  return T(std::numeric_limits<scalar_t<T>>::lowest());
}

template<some_parameter_type T>
constexpr auto highest() -> T
{
  return T(std::numeric_limits<scalar_t<T>>::max());
}

// FNV-1a over the name, continued over the type code. Retyping a parameter
// under the same name would otherwise go unnoticed — same four bytes, same
// identifier, garbage restored into a live value — while mixing the type in
// makes the stored cell simply not match, and the parameter comes up with
// its default.
constexpr auto identify(std::string_view name, value_type type) -> std::uint32_t
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

// The optional half of a declaration. Bounds default to the widest the type
// admits, which for a wrapper is whatever its constructor lets through:
// emb::clamped brings its own limits, so a per-unit parameter needs no
// bounds spelled out at all.
template<some_parameter_type T>
struct options {
  T min = detail::lowest<T>();
  T max = detail::highest<T>();
  group_id group{};
  apply_policy apply = apply_policy::on_restart;
  bool writable = true;
  bool expose = true;
};

// A declared parameter on its way into a schema: the erased descriptor plus
// the static type, which a pack of these recovers as a typelist. Named
// apart from schema::parameter, which is what a lookup by name yields.
template<some_parameter_type T>
struct declaration {
  descriptor desc;
};

// param("motor.p", std::int32_t{11}, {.min = 1, .max = 64, .group = ...})
//
// The type comes from the default, so a literal needs its suffix: 0.05f,
// not 0.05, and std::int32_t{1}, not 1. Bounds are checked here rather than
// in make_schema so that a bad declaration is reported on its own line.
template<some_parameter_type T>
consteval auto param(std::string_view name, T def, options<T> opts = {})
    -> declaration<T>
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
