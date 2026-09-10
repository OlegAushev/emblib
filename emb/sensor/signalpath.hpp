#pragma once

#include <emb/signal/path.hpp>

namespace emb::sensor {

// A signal path read the measuring way: calling one turns a raw code into the
// quantity at the head of the path, which is the plain-callable shape
// some_converter asks for.
template<typename... Stages>
struct signalpath : signal::path<Stages...> {
  using signal::path<Stages...>::path;

  constexpr auto operator()(auto code) const
  {
    return this->inverse(code);
  }
};

} // namespace emb::sensor
