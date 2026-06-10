#pragma once

#include <concepts>

namespace emb::actuator {

// Encoder: maps a desired actuator state to a low-level driver command
template<typename E, typename State>
concept some_encoder = std::invocable<E&, State>;

// Driver: applies an encoded command to the hardware
template<typename D, typename Command>
concept some_driver = std::invocable<D&, Command>;

} // namespace emb::actuator
