#pragma once

#include <emb/actuator/concepts.hpp>

#include <type_traits>
#include <utility>

namespace emb::actuator {

// A feedback channel: reads a low-level measurement through a sensor and
// decodes it into the actual actuator state. The mirror image of the command
// path, where an encoder turns a state into a driver command.
template<typename State, typename Sensor, typename Decoder>
  requires some_sensor<Sensor>
        && some_decoder<Decoder, std::invoke_result_t<Sensor&>>
        && std::convertible_to<
               std::invoke_result_t<Decoder&, std::invoke_result_t<Sensor&>>,
               State>
class feedback {
public:
  using state_type = State;
  using reading_type = std::invoke_result_t<Sensor&>;
private:
  Sensor sensor_;
  Decoder decoder_;
public:
  feedback(Sensor sensor, Decoder decoder)
      : sensor_(std::move(sensor)), decoder_(std::move(decoder)) {}

  State read() {
    return decoder_(sensor_());
  }
};

} // namespace emb::actuator
