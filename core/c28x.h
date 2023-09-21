#pragma once


#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <driverlib.h>


namespace emb {

namespace c28x {


template <typename T>
void from_bytes(T& dest, const uint8_t* src) {
    uint16_t c28_bytes[sizeof(T)];
    for (int i = 0; i < sizeof(T); ++i) {
        c28_bytes[i] = src[2*i] | src[2*i+1] << 8;
    }
    memcpy (&dest, &c28_bytes, sizeof(T));
}


template <typename T>
void to_bytes(uint8_t* dest, const T& src) {
    uint16_t c28_bytes[sizeof(T)];
    memcpy(&c28_bytes, &src, sizeof(T));
    for (int i = 0; i < sizeof(T); ++i) {
        dest[2*i] = c28_bytes[i] & 0x00FF;
        dest[2*i+1] = c28_bytes[i] >> 8;
    }
}


template <typename T>
bool are_equal(const T& obj1, const T& obj2) {
    uint8_t obj1_bytes[sizeof(T)*2];
    uint8_t obj2_bytes[sizeof(T)*2];

    to_bytes<T>(obj1_bytes, obj1);
    to_bytes<T>(obj2_bytes, obj2);

    for(int i = 0; i < sizeof(T)*2; ++i) {
        if (obj1_bytes[i] != obj2_bytes[i]) {
            return false;
        }
    }
    return true;
}


} // namespace c28x

} // namespace emb
