#pragma once


#include <emblib/testrunner/testrunner.hpp>
#include <tests/tests_config.hpp>
#include <emblib/array.hpp>
#include <emblib/algorithm.hpp>
#include <emblib/bitset.hpp>
#include <emblib/chrono.hpp>
#include <emblib/circular_buffer.hpp>
#include <emblib/core.hpp>
#include <emblib/memory.hpp>
#include <emblib/filter.hpp>
#include <emblib/math.hpp>
#include <emblib/optional.hpp>
#include <emblib/stack.hpp>
#include <emblib/static_vector.hpp>
#include <emblib/static_string.hpp>
#include <emblib/queue.hpp>
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

