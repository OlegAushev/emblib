#pragma once

#include <emb/i18n.hpp>
#include <emb/panic.hpp>

#include <concepts>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace emb {

// The class template `enum_label` labels a value of the scoped enumeration
// `E`: the value, its name, an identifier for code that describes the value,
// and its title for people, in each language. A label exists at compile time
// only: its constructor and its accessors are `consteval`, so a function that
// reads a label is an immediate function or does not compile, and no code
// brings a table of labels, with its names and titles, into the image by
// reading it at run time. A `constexpr` function template that reads a label
// becomes an immediate function (P2564), and a call of it at run time fails
// to compile where it is made. The address of a label, e.g. `data()` of the
// span of a table, is a constant and can still reach the image: only a check
// of the image catches that.
//
// A table is an array of labels written in braces, which the enumeration's
// hook `enum_labels` returns (see `enum_with_labels`):
//
//   enum class pwm_mode : std::uint8_t { svpwm, dpwm0 };
//
//   consteval std::span<emb::enum_label<pwm_mode> const> enum_labels(pwm_mode)
//   {
//     static constexpr emb::enum_label<pwm_mode> labels[] = {
//         {pwm_mode::svpwm, "svpwm", {"SVPWM", "SVPWM"}},
//         {pwm_mode::dpwm0, "dpwm0", {"DPWM0", "DPWM0"}},
//     };
//     return labels;
//   }
template<typename E>
  requires std::is_scoped_enum_v<E>
class enum_label {
public:
  consteval enum_label(E value, std::string_view name, i18n title)
      : value_(value),
        name_(name),
        title_(title)
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

  consteval i18n title() const
  {
    return title_;
  }

private:
  E value_;
  std::string_view name_;
  i18n title_;
};

// The concept `enum_with_labels<E>` is satisfied if and only if `E` is a
// scoped enumeration that labels its values: `enum_labels` called unqualified
// with a value of `E` returns `std::span<enum_label<E> const>`, the table of
// its labels. An enumeration opts in as a wrapper does for `value_of` (see
// `some_wrapper`): by declaring a `consteval` overload of `enum_labels` that
// argument-dependent lookup finds, in the namespace of the enumeration. Code
// that reads the table takes it from `enum_labels_of`, which checks it.
//
// Only a scoped enumeration is admitted: an unscoped one may have no fixed
// underlying type, and then converting to it a number outside the range of
// its values has undefined behavior and fails constant evaluation, as code
// that converts a number it received, e.g. from a cell of settings, cannot
// rule out.
template<typename E>
concept enum_with_labels = std::is_scoped_enum_v<E> && requires {
  { enum_labels(E{}) } -> std::same_as<std::span<enum_label<E> const>>;
};

// Returns the table of the labels of the values of `E`, as its hook
// `enum_labels` returns it, after checking it: the table is not empty, its
// values, compared as their underlying type, strictly increase, and no two
// labels have the same name. Constant evaluation fails otherwise, and the
// diagnostic names the rule broken. The first label of the table thus holds
// the lowest value labelled and the last one the highest, and a value has at
// most one label; values between them may have none. What a name and a title
// may hold is for the code that shows them to check.
template<enum_with_labels E>
consteval std::span<enum_label<E> const> enum_labels_of()
{
  auto const labels = enum_labels(E{});
  if (labels.empty()) EMB_PANIC("the table of labels is empty");
  for (auto i = 1uz; i < labels.size(); ++i) {
    if (std::to_underlying(labels[i - 1].value())
        >= std::to_underlying(labels[i].value())) {
      EMB_PANIC("the values of the labels do not strictly increase");
    }
  }
  for (auto i = 0uz; i < labels.size(); ++i) {
    for (auto j = 0uz; j < i; ++j) {
      if (labels[j].name() == labels[i].name()) {
        EMB_PANIC("two labels have the same name");
      }
    }
  }
  return labels;
}

} // namespace emb
