#include <emb/core.hpp>

#ifndef EMBLIB_C28X
namespace emb {

__attribute__((weak)) void fatal_error_cb(char const* hint, int code) {
  // do nothing
}

} // namespace emb
#endif
