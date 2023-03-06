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
	invalid_address,
	data_corrupted

}
SCOPED_ENUM_DECLARE_END(Error)


class DriverInterface
{
public:
	virtual Error read(uint16_t page, uint16_t addr, uint8_t* buf, size_t len, emb::chrono::milliseconds timeout) = 0;
	virtual Error write(uint16_t page, uint16_t addr, const uint8_t* buf, size_t len, emb::chrono::milliseconds timeout) = 0;
};


class Storage
{
private:
	DriverInterface* _driver;
	const size_t page_bytes;
	const size_t page_count;
	uint32_t (*_calc_crc32)(const uint8_t*, size_t);

	const size_t available_page_bytes;
	const size_t available_page_count;

	struct
	{
		uint32_t read;
		uint32_t write;
		uint32_t crc_mismatch;

		uint32_t primary_data_corrupted;
		uint32_t secondary_data_corrupted;
		uint32_t fatal;
	} _errors;
public:
	Storage(DriverInterface* driver_, size_t page_bytes_, size_t page_count_, uint32_t (*calc_crc32_func_)(const uint8_t*, size_t));
	Error read(uint16_t page, uint8_t* buf, size_t len, emb::chrono::milliseconds timeout);
	Error write(uint16_t page, const uint8_t* buf, size_t len, emb::chrono::milliseconds timeout);

	template <typename T>
	Error read(uint16_t page, T& data, emb::chrono::milliseconds timeout)
	{
		uint8_t data_bytes[2*sizeof(T)];
		Error error = read(page, data_bytes, 2*sizeof(T), timeout);
		emb::c28x::from_bytes<T>(data, data_bytes);
		return error;
	}

	template <typename T>
	Error write(uint16_t page, const T& data, emb::chrono::milliseconds timeout)
	{
		uint8_t data_bytes[2*sizeof(T)];
		emb::c28x::to_bytes<T>(data_bytes, data);
		return write(page, data_bytes, 2*sizeof(T), timeout);
	}
};

} // namespace eeprom

} // namespace emb

