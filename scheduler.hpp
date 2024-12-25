#pragma once

#if __cplusplus < 201100
#include <emblib/scheduler/scheduler_cpp03.hpp>
#elif __cplusplus >=201700
#include <emblib/scheduler/scheduler_cpp17.hpp>
#endif
