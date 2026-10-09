#pragma once

#include <emb/filter/concepts.hpp>
#include <emb/meta/all_same.hpp>
#include <emb/meta/unroll.hpp>

#include <cstddef>
#include <tuple>
#include <utility>

namespace emb {

// The class template `cascaded_filter` is a filter that connects the filters
// `Stages` in series. `push(input)` pushes `input` into the first stage and the
// new output of each stage into the next one, and the output is the output of
// the last stage. `set_output(value)` and `reset()` make the same call on every
// stage, and each exists only if every stage has a member function of that
// name.
//
// The stage objects are kept in the public tuple `stages`, so that a stage can
// be read and changed, e.g. by calling `set_timestep` on an
// `exponential_filter` stage.
template<some_filter... Stages>
  requires(sizeof...(Stages) > 0) && all_same<typename Stages::value_type...>
class cascaded_filter {
public:
  using value_type = std::tuple_element_t<0, std::tuple<Stages...>>::value_type;

  static constexpr std::size_t stage_count = sizeof...(Stages);

  std::tuple<Stages...> stages;

  constexpr cascaded_filter() = default;

  constexpr explicit cascaded_filter(Stages... s) : stages(std::move(s)...) {}

  constexpr void push(value_type input)
  {
    unroll<stage_count>([&]<std::size_t I>() {
      auto& stage = std::get<I>(stages);
      stage.push(input);
      input = stage.output();
    });
  }

  constexpr value_type output() const
  {
    return std::get<stage_count - 1>(stages).output();
  }

  constexpr void set_output(value_type value)
    requires(requires(Stages& s, value_type v) { s.set_output(v); } && ...)
  {
    std::apply([&](auto&... s) { (s.set_output(value), ...); }, stages);
  }

  constexpr void reset()
    requires(requires(Stages& s) { s.reset(); } && ...)
  {
    std::apply([](auto&... s) { (s.reset(), ...); }, stages);
  }
};

} // namespace emb
