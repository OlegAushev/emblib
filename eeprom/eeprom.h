#pragma once


#include "../core.h"
#if defined(EMBLIB_C28X)
#include "../chrono.h"
#else
#include <chrono>
#endif
#include <cstring>


namespace emb {

namespace eeprom {


#if defined(EMBLIB_C28X)


SCOPED_ENUM_DECLARE_BEGIN(Status) {
    ok,
    read_failed,
    write_failed,
    read_timeout,
    write_timeout,
    invalid_address,
    invalid_data_size,
    data_corrupted,
    no_device,
} SCOPED_ENUM_DECLARE_END(Status)


#else


enum class Status {
    ok,
    read_failed,
    write_failed,
    read_timeout,
    write_timeout,
    invalid_address,
    invalid_data_size,
    data_corrupted,
    no_device,
};


#endif


class Driver {
public:
    virtual Status read(size_t page, size_t offset, uint8_t* buf, size_t len, EMB_MILLISECONDS timeout) = 0;
    virtual Status write(size_t page, size_t offset, const uint8_t* buf, size_t len, EMB_MILLISECONDS timeout) = 0;
    virtual size_t page_bytes() const = 0;
    virtual size_t page_count() const = 0;
};


class Storage {
private:
    Driver& _driver;
    uint32_t (*_calc_crc32)(const uint8_t*, size_t);

    const size_t available_page_bytes;
    const size_t available_page_count;

    uint8_t* _backup_buf;

    struct {
        uint32_t read;
        uint32_t write;
        uint32_t crc_mismatch;

        uint32_t primary_data_corrupted;
        uint32_t secondary_data_corrupted;
        uint32_t fatal;
    } _errors;
public:
    Storage(Driver& driver_, uint32_t (*calc_crc32_func_)(const uint8_t*, size_t));
    ~Storage();
    Status read(size_t page, uint8_t* buf, size_t len, EMB_MILLISECONDS timeout);
    Status write(size_t page, const uint8_t* buf, size_t len, EMB_MILLISECONDS timeout);

    template <typename T>
    Status read(size_t page, T& data, EMB_MILLISECONDS timeout) {
#if defined(EMBLIB_C28X)
        uint8_t data_bytes[2*sizeof(T)];
        Status error = read(page, data_bytes, 2*sizeof(T), timeout);
        emb::c28x::from_bytes<T>(data, data_bytes);
#else
        uint8_t data_bytes[sizeof(T)];
        Status error = read(page, data_bytes, sizeof(T), timeout);
        memcpy(&data, data_bytes, sizeof(T));
#endif
        return error;
    }

    template <typename T>
    Status write(size_t page, const T& data, EMB_MILLISECONDS timeout) {
#if defined(EMBLIB_C28X)
        uint8_t data_bytes[2*sizeof(T)];
        emb::c28x::to_bytes<T>(data_bytes, data);
        return write(page, data_bytes, 2*sizeof(T), timeout);
#else
        uint8_t data_bytes[sizeof(T)];
        memcpy(data_bytes, &data, sizeof(T));
        return write(page, data_bytes, sizeof(T), timeout);
#endif
    }
};


} // namespace eeprom

} // namespace emb
