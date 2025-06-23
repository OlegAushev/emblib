#pragma once

#include <emblib/core.hpp>
#include <emblib/noncopyable.hpp>
#include <emblib/scopedenum.hpp>

namespace emb {
namespace uart {

class tty {
public:
  tty() EMB_DEFAULT virtual ~tty() EMB_DEFAULT

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
  virtual int send(char const* buf, size_t len) = 0;

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
  virtual int send(char const* buf, size_t len) = 0;

  virtual void register_rx_interrupt_handler(void (*handler)(void)) = 0;
  virtual void enable_rx_interrupts() = 0;
  virtual void disable_rx_interrupts() = 0;
  virtual void acknowledge_rx_interrupt() = 0;
};

#endif

} // namespace uart
} // namespace emb
