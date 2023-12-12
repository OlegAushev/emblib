#include <emblib/core.h>


#if defined(EMBLIB_ARM)
namespace emb {


__attribute__((weak)) void fatal_error_cb(const char* hint, int code) { /* DO NOTHING */ }


} // namespace emb
#endif
