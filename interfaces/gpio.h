#pragma once


#include "../core.h"


namespace emb {


namespace gpio {


#if defined(EMBLIB_C28X)


SCOPED_ENUM_DECLARE_BEGIN(active_state) {
    low = 0,
    high = 1
} SCOPED_ENUM_DECLARE_END(active_state)


SCOPED_ENUM_DECLARE_BEGIN(state) {
    inactive = 0,
    active = 1
} SCOPED_ENUM_DECLARE_END(state)


#elif defined(EMBLIB_ARM)


enum class active_state : unsigned int {
    low = 0,
    high = 1
};


enum class state : unsigned int {
    inactive = 0,
    active = 1
};


#endif


class input {
public:
    input() EMB_DEFAULT
    virtual ~input() EMB_DEFAULT

    virtual state read() const = 0;
    virtual unsigned int read_level() const = 0;
};


class output {
public:
    output() EMB_DEFAULT
    virtual ~output() EMB_DEFAULT

    virtual state read() const = 0;
    virtual void set(state st = state::active) = 0;
    virtual void reset() = 0;
    virtual void toggle() = 0;
    virtual unsigned int read_level() const = 0;
    virtual void set_level(unsigned int level) = 0;
};


} // namespace gpio


} // namespace emb
