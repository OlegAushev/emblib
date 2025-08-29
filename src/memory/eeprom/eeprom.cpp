#include "eeprom.hpp"

namespace emb {
namespace mem {
namespace eeprom {

storage::storage(driver& driver_,
                 uint32_t (*calc_crc32_func_)(uint8_t const*, size_t))
    : _driver(driver_),
      _calc_crc32(calc_crc32_func_),
      available_page_bytes(_driver.page_bytes() - 4),
      available_page_count((_driver.page_count() - 2) / 2) {
  _errors.read = 0;
  _errors.write = 0;
  _errors.crc_mismatch = 0;

  _errors.primary_data_corrupted = 0;
  _errors.secondary_data_corrupted = 0;
  _errors.fatal = 0;

  _backup_buf = new uint8_t[available_page_bytes];
}

storage::~storage() {
  delete[] _backup_buf;
}

emb::mem::status storage::write(size_t page,
                                uint8_t const* buf,
                                size_t len,
                                EMB_MILLISECONDS timeout) {
  assert(page < available_page_count);
  assert(len < available_page_bytes);

  if (page >= available_page_count) {
    return emb::mem::status::invalid_address;
  }

  if (len >= available_page_bytes) {
    return emb::mem::status::invalid_data_size;
  }

  uint32_t crc = _calc_crc32(buf, len);
  uint8_t crc_bytes[4];
#ifdef __c28x__
  emb::c28x::to_bytes<uint32_t>(crc_bytes, crc);
#else
  memcpy(crc_bytes, &crc, 4);
#endif

  emb::mem::status sts = _driver.write(page, 0, buf, len, timeout);
  if (sts != emb::mem::status::ok) {
    ++_errors.write;
    goto write_end;
  }

  sts = _driver.write(page, len, crc_bytes, 4, timeout);
  if (sts != emb::mem::status::ok) {
    ++_errors.write;
    goto write_end;
  }

  sts = _driver.write(page + available_page_count, 0, buf, len, timeout);
  if (sts != emb::mem::status::ok) {
    ++_errors.write;
    goto write_end;
  }

  sts = _driver.write(page + available_page_count, len, crc_bytes, 4, timeout);
  if (sts != emb::mem::status::ok) {
    ++_errors.write;
    goto write_end;
  }

write_end:
  return sts;
}

emb::mem::status
storage::read(size_t page, uint8_t* buf, size_t len, EMB_MILLISECONDS timeout) {
  assert(page < available_page_count);
  assert(len < available_page_bytes);

  if (page >= available_page_count) {
    return emb::mem::status::invalid_address;
  }

  if (len >= available_page_bytes) {
    return emb::mem::status::invalid_data_size;
  }

  uint8_t crc_bytes[4];
  uint32_t primary_crc = 0;
  uint32_t primary_stored_crc = 0;
  uint32_t secondary_crc = 0;
  uint32_t secondary_stored_crc = 0;

  bool primary_ok = false;
  bool secondary_ok = false;

  emb::mem::status sts = _driver.read(page, 0, buf, len, timeout);
  if (sts != emb::mem::status::ok) {
    ++_errors.read;
    goto read_backup;
  }

  sts = _driver.read(page, len, crc_bytes, 4, timeout);
  if (sts != emb::mem::status::ok) {
    ++_errors.read;
    goto read_backup;
  }

#ifdef __c28x__
  emb::c28x::from_bytes<uint32_t>(primary_stored_crc, crc_bytes);
#else
  memcpy(&primary_stored_crc, crc_bytes, 4);
#endif
  primary_crc = _calc_crc32(buf, len);
  if (primary_crc == primary_stored_crc) {
    primary_ok = true;
  } else {
    ++_errors.crc_mismatch;
  }

read_backup:
  sts = _driver.read(page + available_page_count, 0, _backup_buf, len, timeout);
  if (sts != emb::mem::status::ok) {
    ++_errors.read;
    goto read_end;
  }

  sts = _driver.read(page + available_page_count, len, crc_bytes, 4, timeout);
  if (sts != emb::mem::status::ok) {
    ++_errors.read;
    goto read_end;
  }

#ifdef __c28x__
  emb::c28x::from_bytes<uint32_t>(secondary_stored_crc, crc_bytes);
#else
  memcpy(&secondary_stored_crc, crc_bytes, 4);
#endif
  secondary_crc = _calc_crc32(_backup_buf, len);
  if (secondary_crc == secondary_stored_crc) {
    secondary_ok = true;
  } else {
    ++_errors.crc_mismatch;
  }

read_end:
  if (primary_ok && secondary_ok && (primary_crc == secondary_crc)) {
    return emb::mem::status::ok;
  } else if ((primary_ok && !secondary_ok) ||
             (primary_ok && secondary_ok && (primary_crc != secondary_crc))) {
    // backup is corrupted or outdated
    ++_errors.secondary_data_corrupted;
    _driver.write(page + available_page_count, 0, buf, len, timeout);
#ifdef __c28x__
    emb::c28x::to_bytes<uint32_t>(crc_bytes, primary_crc);
#else
    memcpy(crc_bytes, &primary_crc, 4);
#endif
    _driver.write(page + available_page_count, len, crc_bytes, 4, timeout);
    return emb::mem::status::ok;
  } else if (!primary_ok && secondary_ok) {
    // restore backup
    ++_errors.primary_data_corrupted;
    memcpy(buf, _backup_buf, len); // update output buffer
    _driver.write(page, 0, _backup_buf, len, timeout);
#ifdef __c28x__
    emb::c28x::to_bytes<uint32_t>(crc_bytes, secondary_crc);
#else
    memcpy(crc_bytes, &secondary_crc, 4);
#endif
    _driver.write(page, len, crc_bytes, 4, timeout);
    return emb::mem::status::ok;
  } else if (sts == emb::mem::status::ok) {
    ++_errors.fatal;
    return emb::mem::status::data_corrupted;
  }

  return sts;
}

} // namespace eeprom
} // namespace mem
} // namespace emb
