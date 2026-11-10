#pragma once

#include <concepts>

namespace emb {

// The concept `some_filter<F>` specifies that `F` is a filter, i.e. a type
// whose objects take input values of type `F::value_type`, one per call to
// `filter_feed`, and provide an output of the same type, which
// `filter_output` returns. It is satisfied if and only if `F::value_type`
// names a type, `filter_feed` called unqualified with an lvalue of type `F`
// and a const lvalue of type `F::value_type` is valid, and `filter_output`
// called unqualified with a const lvalue of type `F` returns a value
// convertible to `F::value_type`. A reference type does not satisfy it.
//
// A filter opts in by declaring both hooks where argument-dependent lookup
// finds them, as the filters of emblib do with friends defined in the class.
// The hooks call the members that name the operations for the class, e.g.
// `push` and `output` in `median_filter`, `step` and `output` in
// `exponential_filter`. Generic code, e.g. `sensor::singlechannel`, calls the
// hooks; code that knows the type of the filter calls its members.
template<typename F>
concept some_filter =
    requires(F f, F const cf, typename F::value_type const v) {
      typename F::value_type;
      { filter_output(cf) } -> std::convertible_to<typename F::value_type>;
      filter_feed(f, v);
    };

} // namespace emb
