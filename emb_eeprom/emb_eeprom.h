#pragma once


#include "../emb_core.h"
#include "../emb_chrono.h"


namespace emb {

namespace eeprom {

SCOPED_ENUM_DECLARE_BEGIN(Error)
{
	no_error,
	read_failed,
	write_failed,
	read_timeout,
	write_timeout,
}
SCOPED_ENUM_DECLARE_END(Error)


class IController
{
public:
	virtual Error read(uint16_t page, uint16_t addr, char* buf, size_t len, emb::chrono::milliseconds timeout) = 0;
	virtual Error write(uint16_t page, uint16_t addr, const char* buf, size_t len, emb::chrono::milliseconds timeout) = 0;
};


class Storage
{
private:
	IController* _controller;
	const size_t page_bytes;
	const size_t page_count;
public:
	Storage(IController* controller_, size_t bytes_per_page_, size_t page_count_);
};

} // namespace eeprom

} // namespace emb

