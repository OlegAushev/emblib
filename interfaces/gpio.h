#pragma once

#include "../core.h"

namespace emb {
namespace gpio {

#if defined(EMBLIB_C28X)

// OBSOLETE: moved to mcu::gpio
//SCOPED_ENUM_DECLARE_BEGIN(active_state) {
//    low = 0,
//    high = 1
//} SCOPED_ENUM_DECLARE_END(active_state)
//
//
//SCOPED_ENUM_DECLARE_BEGIN(pin_state) {
//    inactive = 0,
//    active = 1
//} SCOPED_ENUM_DECLARE_END(pin_state)

#elif defined(EMBLIB_ARM)

enum class active_state : unsigned int {
    low = 0,
    high = 1
};

enum class pin_state : unsigned int {
    inactive = 0,
    active = 1
};

#endif

#if defined(EMBLIB_ARM)
class input_pin {
public:
    input_pin() EMB_DEFAULT
    virtual ~input_pin() EMB_DEFAULT

    virtual pin_state read() const = 0;
    virtual unsigned int read_level() const = 0;
};

class output_pin {
public:
    output_pin() EMB_DEFAULT
    virtual ~output_pin() EMB_DEFAULT

    virtual pin_state read() const = 0;
    virtual void set(pin_state s = pin_state::active) = 0;
    virtual void reset() = 0;
    virtual void toggle() = 0;
    virtual unsigned int read_level() const = 0;
    virtual void set_level(unsigned int level) = 0;
};
#endif

} // namespace gpio
} // namespace emb
