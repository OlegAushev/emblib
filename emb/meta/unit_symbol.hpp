#pragma once

#include <emb/i18n.hpp>

#include <concepts>

namespace emb {

// The concept `unit_with_symbol<T>` is satisfied if and only if `T` is a unit
// that names its symbol, in each language of `i18n::text`, i.e. `unit_symbol`
// called unqualified with a `T const` lvalue returns `i18n::text`, e.g. "V"
// and "В" for `emb::units::volt_f32` and "pu" and "о. е." for
// `emb::signed_pu_f32`.
//
// A type opts in as it does for `value_of` (see `some_wrapper`): by declaring
// a `unit_symbol` overload that argument-dependent lookup finds, in its own
// namespace or as a friend defined in the class. Generic code, e.g. a
// transport that shows a value with its unit, then needs to know neither the
// type nor where it is declared.
template<typename T>
concept unit_with_symbol = requires(T const& v) {
  { unit_symbol(v) } -> std::same_as<i18n::text>;
};

// Returns the symbol of the unit of `T`, in each language, if `T` satisfies
// `unit_with_symbol`, otherwise a text empty in each language, `i18n::text{}`.
// The symbol belongs to the type, not to a value: `unit_symbol` is called
// with a value-initialized `T`, so the program is ill-formed if such a `T`
// cannot be default-constructed.
template<typename T>
constexpr i18n::text unit_symbol_of()
{
  if constexpr (unit_with_symbol<T>) {
    return unit_symbol(T{});
  }
  else {
    return {};
  }
}

} // namespace emb
