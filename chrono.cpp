#include <emblib/chrono.hpp>


namespace emb {


namespace chrono {


EMB_MILLISECONDS (*steady_clock::_now)() = steady_clock::_default_now_getter;
bool steady_clock::_initialized = false;


} // namespace chrono


} // namespace emb
