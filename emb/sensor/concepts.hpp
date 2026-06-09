#pragma once

#include <concepts>
#include <optional>

namespace emb::sensor {

template<typename Q>
concept some_spsc_queue = requires(Q q, typename Q::value_type v) {
  typename Q::value_type;
  { q.try_push(v) } -> std::convertible_to<bool>;
  { q.try_pop() } -> std::same_as<std::optional<typename Q::value_type>>;
};

template<typename C, typename Raw, typename Value>
concept some_converter = requires(C c, Raw raw) {
  { c(raw) } -> std::convertible_to<Value>;
};

template<typename F>
concept some_filter = requires(
    F f,
    F const cf,
    typename F::value_type const v
) {
  typename F::value_type;
  { cf.output() } -> std::convertible_to<typename F::value_type>;
  f.push(v);
};

} // namespace emb::sensor
