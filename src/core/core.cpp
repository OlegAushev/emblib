#include <emb/core.hpp>

#ifndef __c28x__
namespace emb {

__attribute__((weak)) void fatal_error_cb(char const* hint, int code) {
  // do nothing
}

} // namespace emb
#endif
