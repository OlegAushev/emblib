#pragma once


#if defined(EMBLIB_C28X)
#include <emblib/scheduler/scheduler_c28x.hpp>
#elif defined(EMBLIB_ARM)
#include <emblib/scheduler/scheduler_arm.hpp>
#endif
