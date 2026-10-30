#pragma once

#include <emb/i18n.hpp>
#include <emb/panic.hpp>

#include <concepts>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace emb {

// The class template `enumerator_info` describes an enumerator of the scoped
// enumeration `E`: its value, its name, an identifier for code that describes
// the value, and its label for people, in each language. An
// `enumerator_info` exists at compile time only: its constructor and its
// accessors are `consteval`, so a function that reads one is an immediate
// function or does not compile, and no code brings a table of them, with
// their names and labels, into the image by reading it at run time. A
// `constexpr` function template that reads one becomes an immediate function
// (P2564), and a call of it at run time fails to compile where it is made.
// The address of one, e.g. `data()` of the span of a table, is a constant and
// can still reach the image: only a check of the image catches that.
//
// A table is an array of them written in braces, which the enumeration's
// hook `enum_info` returns (see `enum_with_info`):
//
//   enum class pwm_mode : std::uint8_t { svpwm, dpwm0 };
//
//   consteval std::span<emb::enumerator_info<pwm_mode> const>
//   enum_info(pwm_mode)
//   {
//     static constexpr emb::enumerator_info<pwm_mode> info[] = {
//         {pwm_mode::svpwm, "svpwm", {"SVPWM", "SVPWM"}},
//         {pwm_mode::dpwm0, "dpwm0", {"DPWM0", "DPWM0"}},
//     };
//     return info;
//   }
template<typename E>
  requires std::is_scoped_enum_v<E>
class enumerator_info {
public:
  consteval enumerator_info(E value, std::string_view name, i18n::text label)
      : value_(value),
        name_(name),
        label_(label)
  {
  }

  consteval E value() const
  {
    return value_;
  }

  consteval std::string_view name() const
  {
    return name_;
  }

  consteval i18n::text label() const
  {
    return label_;
  }

private:
  E value_;
  std::string_view name_;
  i18n::text label_;
};

// The concept `enum_with_info<E>` is satisfied if and only if `E` is a scoped
// enumeration that labels its values: `enum_info` called unqualified with a
// value of `E` returns `std::span<enumerator_info<E> const>`, the info of its
// enumerators. An enumeration opts in as a wrapper does for `value_of` (see
// `some_wrapper`): by declaring a `consteval` overload of `enum_info` that
// argument-dependent lookup finds, in the namespace of the enumeration. Code
// that reads the table takes it from `enum_info_of`, which checks it.
//
// Only a scoped enumeration is admitted: an unscoped one may have no fixed
// underlying type, and then converting to it a number outside the range of
// its values has undefined behavior and fails constant evaluation, as code
// that converts a number it received, e.g. from a cell of settings, cannot
// rule out.
template<typename E>
concept enum_with_info = std::is_scoped_enum_v<E> && requires {
  { enum_info(E{}) } -> std::same_as<std::span<enumerator_info<E> const>>;
};

// Returns the info of the enumerators of `E`, as its hook `enum_info` returns
// it, after checking it: the table is not empty, its values, compared as
// their underlying type, strictly increase, and no two enumerators have the
// same name. Constant evaluation fails otherwise, and the diagnostic names
// the rule broken. The first element of the table thus holds the lowest value
// labelled and the last one the highest, and a value has at most one element;
// values between them may have none. What a name and a label may hold is for
// the code that shows them to check.
template<enum_with_info E>
consteval std::span<enumerator_info<E> const> enum_info_of()
{
  auto const info = enum_info(E{});
  if (info.empty()) EMB_PANIC("the table of enumerators is empty");
  for (auto i = 1uz; i < info.size(); ++i) {
    if (std::to_underlying(info[i - 1].value())
        >= std::to_underlying(info[i].value())) {
      EMB_PANIC("the values of the enumerators do not strictly increase");
    }
  }
  for (auto i = 0uz; i < info.size(); ++i) {
    for (auto j = 0uz; j < i; ++j) {
      if (info[j].name() == info[i].name()) {
        EMB_PANIC("two enumerators have the same name");
      }
    }
  }
  return info;
}

} // namespace emb
