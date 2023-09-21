#include <emblib_c28x/core.h>


#if defined(EMBLIB_STM32)
namespace emb {


__attribute__((weak)) void fatal_error_cb(const char* hint, int code) { /* DO NOTHING */ }


} // namespace emb
#endif
