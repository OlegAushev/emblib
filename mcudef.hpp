#pragma once

#include <emblib/core.hpp>
#include <emblib/noncopyable.hpp>
#include <emblib/scopedenum.hpp>

namespace mcu {
namespace gpio {

#if __cplusplus >= 201100

enum class active_state : unsigned int {
    low = 0,
    high = 1
};

enum class pin_state : unsigned int {
    inactive = 0,
    active = 1
};

class input_pin {
public:
    input_pin() = default;
    virtual ~input_pin() = default;

    virtual pin_state read() const = 0;
    virtual unsigned int read_level() const = 0;
};

class output_pin {
public:
    output_pin() = default;
    virtual ~output_pin() = default;

    virtual pin_state read() const = 0;
    virtual void set(pin_state s = pin_state::active) = 0;
    virtual void reset() = 0;
    virtual void toggle() = 0;
    virtual unsigned int read_level() const = 0;
    virtual void set_level(unsigned int level) = 0;
};

#else

SCOPED_ENUM_UT_DECLARE_BEGIN(active_state, unsigned int) {
    low = 0,
    high = 1
} SCOPED_ENUM_DECLARE_END(active_state)

SCOPED_ENUM_UT_DECLARE_BEGIN(pin_state, unsigned int) {
    inactive = 0,
    active = 1
} SCOPED_ENUM_DECLARE_END(pin_state)

class digital_input {
public:
    digital_input() {}
    virtual ~digital_input() {}

    virtual pin_state read() const = 0;
    virtual unsigned int read_level() const = 0;
};

class digital_output {
public:
    digital_output() {}
    virtual ~digital_output() {}

    virtual pin_state read() const = 0;
    virtual void set(pin_state s = pin_state::active) = 0;
    virtual void reset() = 0;
    virtual void toggle() = 0;
    virtual unsigned int read_level() const = 0;
    virtual void set_level(unsigned int level) = 0;
};

#endif

} // namespace gpio

namespace uart {

class tty {
public:
    tty() EMB_DEFAULT
    virtual ~tty() EMB_DEFAULT

    virtual int getchar() = 0;
    virtual int putchar(int ch) = 0;
};

#if __cplusplus >= 201100

class module : protected emb::noncopyable {
public:
    module() = default;
    virtual ~module() = default;

    //virtual void reset() = 0;
    //virtual bool hasRxError() const = 0;

    virtual int getchar(char& ch) = 0;
    virtual int recv(char* buf, size_t len) = 0;

    virtual int putchar(char ch) = 0;
    virtual int send(const char* buf, size_t len) = 0;

    //virtual void registerRxInterruptHandler(void (*handler)(void)) = 0;
    //virtual void enableRxInterrupts() = 0;
    //virtual void disableRxInterrupts() = 0;
    //virtual void acknowledgeRxInterrupt() = 0;
};

#else

class module : protected emb::noncopyable {
public:
    module() {}
    virtual ~module() {}

    virtual void reset() = 0;
    virtual bool has_rx_error() const = 0;

    virtual int getchar(char& ch) = 0;
    virtual int recv(char* buf, size_t buf_len) = 0;

    virtual int putchar(char ch) = 0;
    virtual int send(const char* buf, size_t len) = 0;

    virtual void register_rx_interrupt_handler(void (*handler)(void)) = 0;
    virtual void enable_rx_interrupts() = 0;
    virtual void disable_rx_interrupts() = 0;
    virtual void acknowledge_rx_interrupt() = 0;
};

#endif

} // namespace uart

} // namespace mcu
