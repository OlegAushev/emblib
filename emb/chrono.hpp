#pragma once

#include <emb/chrono/concepts.hpp>
#include <emb/chrono/timebase.hpp>
#include <emb/chrono/timeout.hpp>
#include <emb/chrono/triggered_action.hpp>

#include <chrono>
#include <cstdint>

namespace emb {
namespace chrono {

using nanoseconds_i32 = std::chrono::duration<std::int32_t, std::nano>;
using microseconds_i32 = std::chrono::duration<std::int32_t, std::micro>;
using milliseconds_i32 = std::chrono::duration<std::int32_t, std::milli>;
using seconds_i32 = std::chrono::duration<std::int32_t>;

} // namespace chrono
} // namespace emb
