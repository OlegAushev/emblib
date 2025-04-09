#pragma once

#include <emblib/core.hpp>
#if __cplusplus < 201100
#include <emblib/scopedenum.hpp>
#endif

namespace emb {
namespace mem {

#if __cplusplus >= 201100
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
#else
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
#endif

} // namespace mem
} // namespace emb
