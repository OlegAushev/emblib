#pragma once


#include <emblib/testrunner/testrunner.h>
#include <tests/tests_config.h>
#include <emblib/array.h>
#include <emblib/algorithm.h>
#include <emblib/bitset.h>
#include <emblib/chrono.h>
#include <emblib/circular_buffer.h>
#include <emblib/core.h>
#include <emblib/memory.h>
#include <emblib/filter.h>
#include <emblib/math.h>
#include <emblib/optional.h>
#include <emblib/stack.h>
#include <emblib/static_vector.h>
#include <emblib/static_string.h>
#include <emblib/queue.h>
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

