#include <emb_c28x/emb_profiler/emb_profiler.h>


namespace emb {

uint64_t time_now_func_none_us()
{
	return 0;
}
uint64_t (*DurationLogger_us::_time_now_func)() = time_now_func_none_us;


uint32_t time_now_func_none_clk()
{
	return 0;
}
uint32_t (*DurationLogger_clk::_time_now_func)() = time_now_func_none_clk;


uint64_t (*DurationLoggerAsync_us::_time_now_func)() = time_now_func_none_us;
DurationLoggerAsync_us::DurationData DurationLoggerAsync_us::_durations_us[_capacity];

}

