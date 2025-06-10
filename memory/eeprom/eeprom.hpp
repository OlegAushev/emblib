#pragma once

#include <cstring>
#include <emblib/chrono.hpp>
#include <emblib/core.hpp>
#include <emblib/memory/memory_def.hpp>

namespace emb {
namespace mem {
namespace eeprom {

class driver {
public:
  virtual emb::mem::status read(size_t page,
                                size_t offset,
                                uint8_t* buf,
                                size_t len,
                                EMB_MILLISECONDS timeout) = 0;
  virtual emb::mem::status write(size_t page,
                                 size_t offset,
                                 uint8_t const* buf,
                                 size_t len,
                                 EMB_MILLISECONDS timeout) = 0;
  virtual size_t page_bytes() const = 0;
  virtual size_t page_count() const = 0;
};

class storage {
private:
  driver& _driver;
  uint32_t (*_calc_crc32)(uint8_t const*, size_t);

  size_t const available_page_bytes;
  size_t const available_page_count;

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
  storage(driver& driver_,
          uint32_t (*calc_crc32_func_)(uint8_t const*, size_t));
  ~storage();
  emb::mem::status
  read(size_t page, uint8_t* buf, size_t len, EMB_MILLISECONDS timeout);
  emb::mem::status
  write(size_t page, uint8_t const* buf, size_t len, EMB_MILLISECONDS timeout);

  template<typename T>
  emb::mem::status read(size_t page, T& data, EMB_MILLISECONDS timeout) {
#if defined(EMBLIB_C28X)
    uint8_t data_bytes[2 * sizeof(T)];
    status sts = read(page, data_bytes, 2 * sizeof(T), timeout);
    emb::c28x::from_bytes<T>(data, data_bytes);
#else
    uint8_t data_bytes[sizeof(T)];
    emb::mem::status sts = read(page, data_bytes, sizeof(T), timeout);
    memcpy(&data, data_bytes, sizeof(T));
#endif
    return sts;
  }

  template<typename T>
  emb::mem::status write(size_t page, T const& data, EMB_MILLISECONDS timeout) {
#if defined(EMBLIB_C28X)
    uint8_t data_bytes[2 * sizeof(T)];
    emb::c28x::to_bytes<T>(data_bytes, data);
    return write(page, data_bytes, 2 * sizeof(T), timeout);
#else
    uint8_t data_bytes[sizeof(T)];
    memcpy(data_bytes, &data, sizeof(T));
    return write(page, data_bytes, sizeof(T), timeout);
#endif
  }
};

} // namespace eeprom
} // namespace mem
} // namespace emb
