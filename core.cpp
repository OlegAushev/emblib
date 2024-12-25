#include <emblib/core.hpp>

#if defined(EMBLIB_ARM)
namespace emb {

__attribute__((weak)) void fatal_error_cb(const char* hint, int code) {
    // do nothing
}

} // namespace emb
#endif
