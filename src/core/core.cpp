#include <emb/core.hpp>

#if defined(EMBLIB_ARM)
namespace emb {

__attribute__((weak)) void fatal_error_cb(char const* hint, int code) {
  // do nothing
}

} // namespace emb
#endif
