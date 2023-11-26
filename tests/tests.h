#pragma once


#include "../testrunner/testrunner.h"
#include <tests/tests_config.h>
#include "../array.h"
#include "../algorithm.h"
#include "../bitset.h"
#include "../chrono.h"
#include "../circularbuffer.h"
#include "../core.h"
#include "../eeprom/eeprom.h"
#include "../filter.h"
#include "../math.h"
#include "../optional.h"
#include "../stack.h"
#include "../staticvector.h"
#include "../staticstring.h"
#include "../queue.h"
#include <algorithm>


namespace emb {

class tests {
public:
    static void algorithm_test();
    static void array_test();
    static void chrono_test();
    static void circular_buffer_test();
    static void common_test();
    static void filter_test();
    static void math_test();
    static void optional_test();
    static void stack_test();
    static void static_vector_test();
    static void static_string_test();
    static void queue_test();
#if defined(EMBLIB_C28X)
    static void bitset_test();
#endif
    static void eeprom_test();
};

} // namespace emb

