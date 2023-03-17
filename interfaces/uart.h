#pragma once


#include "../core.h"


namespace emb {

namespace uart {

class UartInterface {
private:
    UartInterface(const UartInterface& other);              // no copy constructor
    UartInterface& operator=(const UartInterface& other);   // no copy assignment operator
public:
    UartInterface() {}
    virtual ~UartInterface() {}

    virtual void reset() = 0;
    virtual bool has_rx_error() const = 0;

    virtual int getchar(char& ch) = 0;
    virtual int recv(char* buf, size_t bufLen) = 0;

    virtual int putchar(char ch) = 0;
    virtual int send(const char* buf, uint16_t len) = 0;

    virtual void register_rx_interrupt_handler(void (*handler)(void)) = 0;
    virtual void enable_rx_interrupts() = 0;
    virtual void disable_rx_interrupts() = 0;
    virtual void acknowledge_rx_interrupt() = 0;
};

} // namespace uart

} // namespace emb

