#pragma once

#include <concepts>

namespace emb {

// The concept `some_filter<F>` specifies that `F` is a filter, i.e. a type
// whose objects take input values of type `F::value_type`, one per call to
// `push`, and provide an output of the same type, which `output()` returns. It
// is satisfied if and only if `F::value_type` names a type, `push` can be
// called on an lvalue of type `F` with a const lvalue of type `F::value_type`,
// and `output` can be called on a const lvalue of type `F` and returns a value
// convertible to `F::value_type`. A reference type does not satisfy it.
template<typename F>
concept some_filter =
    requires(F f, F const cf, typename F::value_type const v) {
      typename F::value_type;
      { cf.output() } -> std::convertible_to<typename F::value_type>;
      f.push(v);
    };

} // namespace emb
