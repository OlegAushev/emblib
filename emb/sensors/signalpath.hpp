#pragma once

#include <emb/signal/path.hpp>

namespace emb::sensors {

// A signal path read the measuring way: calling one turns a raw code into the
// quantity at the head of the path, which is the plain call that singlechannel
// asks of its converter.
template<typename... Stages>
struct signalpath : signal::path<Stages...> {
  using signal::path<Stages...>::path;

  constexpr auto operator()(auto code) const
  {
    return this->inverse(code);
  }
};

} // namespace emb::sensors
