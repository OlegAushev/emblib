#pragma once

#if __cplusplus < 201100
#include "../../src/fsm/fsm_sp_cpp03.hpp"
#elif __cplusplus >= 202300
#include "../../src/fsm/fsm_sp_cpp23.hpp"
#include "../../src/fsm/fsm_v2.hpp"
#include "../../src/fsm/fsm_v3.hpp"
#endif
