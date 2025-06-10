#pragma once

#include <limits.h>

#if CHAR_BIT == 16

#if __cplusplus >= 201100
#include <cstdint>
#else
#include <stdint.h>
#endif
#include <cstddef>
#ifdef EMBLIB_C28X
#include <driverlib.h>
#endif

namespace emb {
namespace c28x {

template<typename T>
void from_bytes(T& dest, uint8_t const* src) {
  uint16_t c28_bytes[sizeof(T)];
  for (size_t i = 0; i < sizeof(T); ++i) {
    c28_bytes[i] = src[2 * i] | src[2 * i + 1] << 8;
  }
  memcpy(&dest, &c28_bytes, sizeof(T));
}

template<typename T>
void to_bytes(uint8_t* dest, T const& src) {
  uint16_t c28_bytes[sizeof(T)];
  memcpy(&c28_bytes, &src, sizeof(T));
  for (size_t i = 0; i < sizeof(T); ++i) {
    dest[2 * i] = c28_bytes[i] & 0x00FF;
    dest[2 * i + 1] = c28_bytes[i] >> 8;
  }
}

} // namespace c28x
} // namespace emb

#endif
