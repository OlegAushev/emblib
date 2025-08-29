#pragma once

#define EMB_CONSTEXPR_ASSERT(expression) \
  if (!(expression)) {                   \
    return false;                        \
  }

#if 0
#include <emb/algorithm.hpp>
#include <emb/array.hpp>
#include <emb/bitset.hpp>
#include <emb/chrono.hpp>
#include <emb/core.hpp>
#include <emb/filter.hpp>
#include <emb/math.hpp>
#include <emb/memory.hpp>
#include <emb/optional.hpp>
#include <emb/queue.hpp>
#include <emb/singleton.hpp>
#include <emb/stack.hpp>
#include <emb/static_string.hpp>
#include <emb/static_vector.hpp>
#include <emb/testrunner/testrunner.hpp>

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
#ifdef __c28x__
  static void bitset_test();
#endif
  static void eeprom_test();
};

} // namespace emb
#endif
