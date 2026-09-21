#pragma once

#include <cstdint>

namespace emb::hall {

enum class error : std::uint8_t {
  invalid_config,      // the settings do not describe a usable sensor
  invalid_calibration, // the measured angles do not describe a usable machine
  invalid_input,       // the inputs read a code no sector owns
};

} // namespace emb::hall
