#pragma once

#define EMB_CONSTEXPR_ASSERT(expression) \
  if (!(expression)) {                   \
    return false;                        \
  }

#include <emblib/algorithm.hpp>
#include <emblib/array.hpp>
#include <emblib/bitset.hpp>
#include <emblib/chrono.hpp>
#include <emblib/core.hpp>
#include <emblib/filter.hpp>
#include <emblib/math.hpp>
#include <emblib/memory.hpp>
#include <emblib/optional.hpp>
#include <emblib/queue.hpp>
#include <emblib/singleton.hpp>
#include <emblib/stack.hpp>
#include <emblib/static_string.hpp>
#include <emblib/static_vector.hpp>
#include <emblib/testrunner/testrunner.hpp>

#include <algorithm>

namespace emb {

class tests {
public:
  static void algorithm_test();
  static void array_test();
  static void chrono_test();
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
