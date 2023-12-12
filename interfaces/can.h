#pragma once


#if defined(EMBLIB_C28X)


#include <emblib/array.h>


typedef emb::array<uint8_t, 8> can_payload;


#elif defined(EMBLIB_ARM)


#include <array>
#include <cstdint>


using can_payload = std::array<uint8_t, 8>;
using can_id = uint32_t;


struct can_frame {
    can_id id;
    uint8_t len;
    can_payload payload;
};


#endif
