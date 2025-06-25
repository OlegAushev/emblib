#include <emblib/chrono.hpp>

namespace emb {
namespace chrono {

#if __cplusplus >= 201100

__attribute__((weak)) std::chrono::time_point<steady_clock>
steady_clock::now() {
  return time_point{};
}

#else

EMB_MILLISECONDS (*steady_clock::_now)() = steady_clock::_default_now_getter;
bool steady_clock::_initialized = false;

#endif

} // namespace chrono
} // namespace emb
