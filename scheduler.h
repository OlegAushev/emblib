#pragma once


#if defined(EMBLIB_C28X)
#include "scheduler/scheduler_c28x.h"
#elif defined(EMBLIB_ARM)
#include "scheduler/scheduler_stm32.h"
#endif
