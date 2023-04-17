#pragma once


#include <tests/tests_config.h>
#include "../algorithm.h"
#include "../array.h"
#include "../bitset.h"
#include "../chrono.h"
#include "../circularbuffer.h"
#include "../core.h"
#include "../filter.h"
#include "../math.h"
#include "../queue.h"
#include "../stack.h"
#include "../staticvector.h"
#include "../string.h"
#include "../testrunner/testrunner.h"
#include "../eeprom/eeprom.h"
#include <algorithm>


namespace emb {

class tests {
public:
    static void common_test();
    static void math_test();
    static void algorithm_test();
    static void array_test();
    static void queue_test();
    static void circular_buffer_test();
    static void filter_test();
    static void stack_test();
    static void bitset_test();
    static void static_vector_test();
    static void string_test();
    static void chrono_test();
    static void eeprom_test();
};

} // namespace emb

