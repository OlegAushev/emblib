#pragma once


#if defined(EMBLIB_C28X)
#include <emblib/scheduler/scheduler_c28x.h>
#elif defined(EMBLIB_ARM)
#include <emblib/scheduler/scheduler_stm32.h>
#endif
