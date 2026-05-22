#pragma once

#include "../memory_def.hpp"
#include <emb/chrono.hpp>

#include <cstring>
#include <cstdint>

namespace emb {
namespace mem {
namespace eeprom {

class driver {
public:
  virtual emb::mem::status read(
      std::size_t page,
      std::size_t offset,
      std::uint8_t* buf,
      std::size_t len,
      std::chrono::milliseconds timeout
  ) = 0;
  virtual emb::mem::status write(
      std::size_t page,
      std::size_t offset,
      std::uint8_t const* buf,
      std::size_t len,
      std::chrono::milliseconds timeout
  ) = 0;
  virtual std::size_t page_bytes() const = 0;
  virtual std::size_t page_count() const = 0;
};

class storage {
private:
  driver& _driver;
  std::uint32_t (*_calc_crc32)(std::uint8_t const*, std::size_t);

  std::size_t const available_page_bytes;
  std::size_t const available_page_count;

  std::uint8_t* _backup_buf;

  struct {
    std::uint32_t read;
    std::uint32_t write;
    std::uint32_t crc_mismatch;

    std::uint32_t primary_data_corrupted;
    std::uint32_t secondary_data_corrupted;
    std::uint32_t fatal;
  } _errors;
public:
  storage(
      driver& driver_,
      std::uint32_t (*calc_crc32_func_)(std::uint8_t const*, std::size_t)
  );
  ~storage();
  emb::mem::status read(
      std::size_t page,
      std::uint8_t* buf,
      std::size_t len,
      std::chrono::milliseconds timeout
  );
  emb::mem::status write(
      std::size_t page,
      std::uint8_t const* buf,
      std::size_t len,
      std::chrono::milliseconds timeout
  );

  template<typename T>
  emb::mem::status
  read(std::size_t page, T& data, std::chrono::milliseconds timeout) {
#ifdef __c28x__
    std::uint8_t data_bytes[2 * sizeof(T)];
    status sts = read(page, data_bytes, 2 * sizeof(T), timeout);
    emb::c28x::from_bytes<T>(data, data_bytes);
#else
    std::uint8_t data_bytes[sizeof(T)];
    emb::mem::status sts = read(page, data_bytes, sizeof(T), timeout);
    memcpy(&data, data_bytes, sizeof(T));
#endif
    return sts;
  }

  template<typename T>
  emb::mem::status
  write(std::size_t page, T const& data, std::chrono::milliseconds timeout) {
#ifdef __c28x__
    std::uint8_t data_bytes[2 * sizeof(T)];
    emb::c28x::to_bytes<T>(data_bytes, data);
    return write(page, data_bytes, 2 * sizeof(T), timeout);
#else
    std::uint8_t data_bytes[sizeof(T)];
    memcpy(data_bytes, &data, sizeof(T));
    return write(page, data_bytes, sizeof(T), timeout);
#endif
  }
};

} // namespace eeprom
} // namespace mem
} // namespace emb
