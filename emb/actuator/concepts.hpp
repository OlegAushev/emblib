#pragma once

#include <concepts>

namespace emb::actuator {

// Encoder: maps a desired actuator state to a low-level driver command
template<typename E, typename State>
concept some_encoder = std::invocable<E&, State>;

// Driver: applies an encoded command to the hardware
template<typename D, typename Command>
concept some_driver = std::invocable<D&, Command>;

// Sensor: reads a low-level measurement back from the hardware
template<typename S>
concept some_sensor = std::invocable<S&>;

// Decoder: maps a sensor reading to an actuator state
template<typename D, typename Reading>
concept some_decoder = std::invocable<D&, Reading>;

} // namespace emb::actuator
