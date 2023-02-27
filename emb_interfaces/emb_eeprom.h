#pragma once


#include "../emb_core.h"


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
SCOPED_ENUM_DECLARE_END(status)


struct Address
{
	unsigned int page;
	unsigned int offset;
	Address() : page(0), offset(0) {}
	explicit Address(unsigned int page_) : page(page_), offset(0) {}
	Address(unsigned int page_, unsigned int offset_) : page(page_), offset(offset_) {}
};


class IController
{
public:
	virtual Error read(Address address, char* buf, size_t len, uint64_t timeout_ms) = 0;
	virtual Error write(Address address, const char* buf, size_t len, uint64_t timeout_ms) = 0;
};


class Storage
{

};

} // namespace eeprom

} // namespace emb

