#pragma once


#include <c28x_emb/emb_core.h>


namespace emb {

class IUart
{
private:
	IUart(const IUart& other);		// no copy constructor
	IUart& operator=(const IUart& other);	// no copy assignment operator
public:
	IUart() {}
	virtual ~IUart() {}

	virtual void reset() = 0;
	virtual bool has_rx_error() const = 0;

	virtual int recv(char& ch) = 0;
	virtual int recv(char* buf, size_t bufLen) = 0;

	virtual int send(char ch) = 0;
	virtual int send(const char* buf, uint16_t len) = 0;

	virtual void register_rx_interrupt_handler(void (*handler)(void)) = 0;
	virtual void enable_rx_interrupts() = 0;
	virtual void disable_rx_interrupts() = 0;
	virtual void acknowledge_rx_interrupt() = 0;
};

} // namespace emb

