#pragma once

#if __cplusplus < 201100
#include "../../src/scheduler/scheduler_cpp03.hpp"
#elif __cplusplus >= 201700
#include "../../src/scheduler/scheduler_cpp17.hpp"
#endif
