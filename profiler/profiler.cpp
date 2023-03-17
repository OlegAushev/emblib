#include <emblib_c28x/profiler/profiler.h>


namespace emb {

emb::chrono::nanoseconds time_now_func_none() {
    return emb::chrono::nanoseconds(0);
}
emb::chrono::nanoseconds (*duration_logger::_time_now_func)() = time_now_func_none;


uint32_t time_now_func_none_clk() {
    return 0;
}
uint32_t (*duration_logger_clk::_time_now_func)() = time_now_func_none_clk;


emb::chrono::nanoseconds (*duration_logger_async::_time_now_func)() = time_now_func_none;
duration_logger_async::DurationData duration_logger_async::_durations_us[_capacity];

}

