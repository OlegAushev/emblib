#include <emblib/eeprom/eeprom.h>


namespace emb {

namespace eeprom {


//#if defined(EMBLIB_C28X)


Storage::Storage(DriverInterface& driver_, uint32_t (*calc_crc32_func_)(const uint8_t*, size_t))
        : _driver(driver_)
        , _calc_crc32(calc_crc32_func_)
        , available_page_bytes(_driver.page_bytes()-4)
        , available_page_count((_driver.page_count()-2)/2) {
    _errors.read = 0;
    _errors.write = 0;
    _errors.crc_mismatch = 0;

    _errors.primary_data_corrupted = 0;
    _errors.secondary_data_corrupted = 0;
    _errors.fatal = 0;
}


#if defined(EMBLIB_C28X)
Error Storage::write(size_t page, const uint8_t* buf, size_t len, emb::chrono::milliseconds timeout) {
#else
Error Storage::write(size_t page, const uint8_t* buf, size_t len, std::chrono::milliseconds timeout) {
#endif
    assert(page < available_page_count);
    assert(len < available_page_bytes);

    if (page >= available_page_count) { return Error::invalid_address; }
    if (len >= available_page_bytes) { return Error::invalid_data_size; }

    uint32_t crc = _calc_crc32(buf, len);
    uint8_t crc_bytes[4];
#if defined(EMBLIB_C28X)
    emb::c28x::to_bytes<uint32_t>(crc_bytes, crc);
#else
    memcpy(crc_bytes, &crc, 4);
#endif

    Error error = _driver.write(page, 0, buf, len, timeout);
    if (error != Error::none) {
        ++_errors.write;
        goto write_end;
    }

    error = _driver.write(page, len, crc_bytes, 4, timeout);
    if (error != Error::none) {
        ++_errors.write;
        goto write_end;
    }

    error = _driver.write(page+available_page_count, 0, buf, len, timeout);
    if (error != Error::none) {
        ++_errors.write;
        goto write_end;
    }

    error = _driver.write(page+available_page_count, len, crc_bytes, 4, timeout);
    if (error != Error::none) {
        ++_errors.write;
        goto write_end;
    }

write_end:
    return error;
}


#if defined(EMBLIB_C28X)
Error Storage::read(size_t page, uint8_t* buf, size_t len, emb::chrono::milliseconds timeout) {
#else
Error Storage::read(size_t page, uint8_t* buf, size_t len, std::chrono::milliseconds timeout) {
#endif
    assert(page < available_page_count);
    assert(len < available_page_bytes);

    if (page >= available_page_count) { return Error::invalid_address; }
    if (len >= available_page_bytes) { return Error::invalid_data_size; }

    uint8_t crc_bytes[4];
    uint32_t primary_crc = 0;
    uint32_t primary_stored_crc = 0;
    uint32_t secondary_crc= 0;
    uint32_t secondary_stored_crc = 0;

    bool primary_ok = false;
    bool secondary_ok = false;

    Error error = _driver.read(page, 0, buf, len, timeout);
    if (error != Error::none) {
        ++_errors.read;
        goto read_backup;
    }

    error = _driver.read(page, len, crc_bytes, 4, timeout);
    if (error != Error::none) {
        ++_errors.read;
        goto read_backup;
    }

#if defined(EMBLIB_C28X)
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
    uint8_t buf_backup[available_page_bytes];

    error = _driver.read(page+available_page_count, 0, buf_backup, len, timeout);
    if (error != Error::none) {
        ++_errors.read;
        goto read_end;
    }

    error = _driver.read(page+available_page_count, len, crc_bytes, 4, timeout);
    if (error != Error::none) {
        ++_errors.read;
        goto read_end;
    }

#if defined(EMBLIB_C28X)
    emb::c28x::from_bytes<uint32_t>(secondary_stored_crc, crc_bytes);
#else
    memcpy(&secondary_stored_crc, crc_bytes, 4);
#endif
    secondary_crc = _calc_crc32(buf_backup, len);
    if (secondary_crc == secondary_stored_crc) {
        secondary_ok = true;
    } else {
        ++_errors.crc_mismatch;
    }

read_end:
    if (primary_ok && secondary_ok && (primary_crc == secondary_crc)) {
        return Error::none;
    } else if ((primary_ok && !secondary_ok) || (primary_ok && secondary_ok && (primary_crc != secondary_crc))) {
        // backup is corrupted or outdated
        ++_errors.secondary_data_corrupted;
        _driver.write(page+available_page_count, 0, buf, len, timeout);
#if defined(EMBLIB_C28X)
        emb::c28x::to_bytes<uint32_t>(crc_bytes, primary_crc);
#else
        memcpy(crc_bytes, &primary_crc, 4);
#endif
        _driver.write(page+available_page_count, len, crc_bytes, 4, timeout);
        return Error::none;
    } else if (!primary_ok && secondary_ok) {
        // restore backup
        ++_errors.primary_data_corrupted;
        memcpy(buf, buf_backup, len); // update output buffer
        _driver.write(page, 0, buf_backup, len, timeout);
#if defined(EMBLIB_C28X)
        emb::c28x::to_bytes<uint32_t>(crc_bytes, secondary_crc);
#else
        memcpy(crc_bytes, &secondary_crc, 4);
#endif
        _driver.write(page, len, crc_bytes, 4, timeout);
        return Error::none;
    } else if (error == Error::none) {
        ++_errors.fatal;
        return Error::data_corrupted;
    }

    return error;
}


//#endif


} // namespace eeprom

} // namespace emb
