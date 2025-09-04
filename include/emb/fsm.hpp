#pragma once

#if __cplusplus < 201100
#include "../../src/fsm/fsm_sp_cpp03.hpp"
#elif __cplusplus >= 202300
#include "../../src/fsm/fsm_sp_cpp23.hpp"
#endif
