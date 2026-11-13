#pragma once

#include <concepts>

namespace emb::filters {

// The concept `filter<F>` specifies that `F` is a filter, i.e. a type
// whose objects take input values of type `F::value_type`, one per call to
// `update`, and provide an output of the same type, which `output()` returns.
// It is satisfied if and only if `F::value_type` names a type, `update` can be
// called on an lvalue of type `F` with a const lvalue of type `F::value_type`,
// and `output` can be called on a const lvalue of type `F` and returns a value
// convertible to `F::value_type`. A reference type does not satisfy it.
//
// Only the name `update` sets a filter apart from a type otherwise alike whose
// output is a quantity other than its input: `integrator` takes a rate through
// `push`, outputs its integral and does not satisfy the concept.
template<typename F>
concept filter = requires(F f, F const cf, typename F::value_type const v) {
  typename F::value_type;
  { cf.output() } -> std::convertible_to<typename F::value_type>;
  f.update(v);
};

} // namespace emb::filters
