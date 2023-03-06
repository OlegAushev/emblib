#include <emblib_c28x/profiler/profiler.h>


namespace emb {

emb::chrono::nanoseconds time_now_func_none()
{
	return emb::chrono::nanoseconds(0);
}
emb::chrono::nanoseconds (*DurationLogger_us::_time_now_func)() = time_now_func_none;


uint32_t time_now_func_none_clk()
{
	return 0;
}
uint32_t (*DurationLogger_clk::_time_now_func)() = time_now_func_none_clk;


emb::chrono::nanoseconds (*DurationLoggerAsync_us::_time_now_func)() = time_now_func_none;
DurationLoggerAsync_us::DurationData DurationLoggerAsync_us::_durations_us[_capacity];

}

