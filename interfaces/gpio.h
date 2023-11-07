#pragma once


#include "../core.h"


namespace emb {

namespace gpio {


#if defined(EMBLIB_C28X)


SCOPED_ENUM_DECLARE_BEGIN(ActiveState) {
    low = 0,
    high = 1
} SCOPED_ENUM_DECLARE_END(ActiveState)


SCOPED_ENUM_DECLARE_BEGIN(State) {
    inactive = 0,
    active = 1
} SCOPED_ENUM_DECLARE_END(State)


#elif defined(EMBLIB_STM32)


enum class ActiveState : unsigned int {
    low = 0,
    high = 1
};


enum class State : unsigned int {
    inactive = 0,
    active = 1
};


#endif


class InputInterface {
public:
    InputInterface() EMB_DEFAULT
    virtual ~InputInterface() EMB_DEFAULT

    virtual State read() const = 0;
    virtual unsigned int read_level() const = 0;
};


class OutputInterface {
public:
    OutputInterface() EMB_DEFAULT
    virtual ~OutputInterface() EMB_DEFAULT

    virtual State read() const = 0;
    virtual void set(State state = State::active) = 0;
    virtual void reset() = 0;
    virtual void toggle() = 0;
    virtual unsigned int read_level() const = 0;
    virtual void set_level(unsigned int level) = 0;
};


} // namespace gpio

} // namespace emb
