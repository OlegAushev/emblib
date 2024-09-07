#pragma once


#include <emblib/core.h>


namespace emb {
namespace mem {


#if defined(EMBLIB_C28X)


SCOPED_ENUM_DECLARE_BEGIN(status) {
    ok,
    read_failed,
    write_failed,
    read_timeout,
    write_timeout,
    invalid_address,
    invalid_data_size,
    data_corrupted,
    no_device,
} SCOPED_ENUM_DECLARE_END(status)


#else


enum class status {
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


} // namespace mem
} // namespace emb
