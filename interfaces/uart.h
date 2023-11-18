#pragma once


#include "../core.h"


namespace emb {

namespace uart {


#if defined(EMBLIB_C28X)


class Uart {
private:
    Uart(const Uart& other);              // no copy constructor
    Uart& operator=(const Uart& other);   // no copy assignment operator
public:
    Uart() {}
    virtual ~Uart() {}

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


#elif defined(EMBLIB_STM32)


class Uart {
public:
    Uart() = default;
    virtual ~Uart() = default;

    Uart(const Uart& other) = delete;
    Uart& operator=(const Uart& other) = delete;

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


#endif


} // namespace uart

} // namespace emb
