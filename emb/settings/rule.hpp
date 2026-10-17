#pragma once

#include <emb/meta/typelist.hpp>
#include <emb/settings/image.hpp>
#include <emb/settings/param.hpp>
#include <emb/settings/schema.hpp>

#include <array>
#include <concepts>

#include <cstddef>

namespace emb {
namespace settings {

namespace detail {

// A type of its own for each declaration, so that declarations of different
// types compare: two are the same declaration exactly if their tags are the
// same type.
template<auto& Decl>
struct declaration_tag {};

template<auto& A, auto& B>
inline constexpr bool same_declaration =
    std::same_as<declaration_tag<A>, declaration_tag<B>>;

} // namespace detail

// The class template `inputs` shows of an image of the schema `Schema` the
// values of the parameters that `Decls` declare, and of no other: `get` of
// another parameter is deleted. A function that takes it reads those values
// alone, so what it returns depends on them alone, which a rule needs
// (`some_rule`). It converts implicitly from an image, to which it refers and
// which must outlive it; it is not built from a temporary image.
template<auto& Schema, auto&... Decls>
  requires(some_declaration<decltype(Decls)> && ...)
class inputs {
public:
  // The indices of the parameters in `Schema`, in the order of `Decls`.
  static constexpr std::array<std::size_t, sizeof...(Decls)> indices{
      parameter<Schema, Decls>::index...};

  constexpr inputs(image<Schema> const& values) : values_(values) {}

  inputs(image<Schema> const&&) = delete;

  // Returns the value of the parameter that `Decl` declares, as `image::get`
  // does, of the type `parameter<Schema, Decl>::type`.
  template<auto& Decl>
    requires(detail::same_declaration<Decl, Decls> || ...)
  constexpr auto get() const
  {
    return values_.template get<Decl>();
  }

  template<auto& Decl>
    requires(!(detail::same_declaration<Decl, Decls> || ...))
  void get() const = delete("the parameter is not an input");

private:
  image<Schema> const& values_;
};

namespace detail {

template<typename T, auto& Schema>
inline constexpr bool is_inputs_of = false;

template<auto& Schema, auto&... Decls>
inline constexpr bool is_inputs_of<inputs<Schema, Decls...>, Schema> = true;

} // namespace detail

// The concept `some_rule<R, Schema>` is satisfied if and only if `R` checks
// values of the schema `Schema` together, which the bounds of each alone
// cannot: `R::in` is a specialization of `inputs` of `Schema`, whose
// parameters are the inputs of the rule, and `R::holds(in)` returns whether
// they may take together the values that `in` shows. A rule reads its inputs
// alone, so one that holds on their defaults holds whenever they are at their
// defaults, whatever the other parameters hold: a section falls back to the
// defaults of the inputs of a rule that breaks
// (`section::fall_back_to_defaults`). A rule should compare values rather
// than compute with them: a computation that the compiler contracts at run
// time, e.g. into a fused multiply-add, may hold on the defaults in constant
// evaluation and break on them at run time.
template<typename R, auto& Schema>
concept some_rule = detail::is_inputs_of<typename R::in, Schema>
                 && requires(typename R::in in) {
                      { R::holds(in) } -> std::same_as<bool>;
                    };

// Checks whether `values` satisfy every rule of `R`.
template<auto& Schema, typename... R>
  requires(some_rule<R, Schema> && ...)
constexpr bool all_hold(typelist<R...>, image<Schema> const& values)
{
  return (R::holds(values) && ...);
}

} // namespace settings
} // namespace emb
