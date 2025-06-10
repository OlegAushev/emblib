#pragma once

#include <cstdio>
#include <cstring>
#include <emblib/core.hpp>

namespace emb {

void run_tests();

class test_runner {
#if defined(EMBLIB_C28X)
public:
  static void (*print)(char const* str);
  static void (*print_nextline)();
private:
  static void print_dbg(char const* str) { printf(str); }

  static void print_nextline_dbg() { printf("\n"); }
#elif defined(EMBLIB_ARM)
public:
  static inline void (*print)(char const* str) = [](char const* str) {
    fatal_error("emb::test_runner print function not defined");
  };

  static inline void (*print_nextline)() = []() {
    fatal_error("emb::test_runner print_nextline function not defined");
  };
#endif
private:
  static int _asserts_in_test;
  static int _asserts;
  static int _asserts_failed_in_test;
  static int _asserts_failed;
  static int _tests;
  static int _tests_failed;
  static int _tests_passed;
  static int _tests_skipped;
public:
  template<typename T, typename U>
  static void assert_equal(T const& t, U const& u, char const* hint) {
    ++_asserts_in_test;
    ++_asserts;
    if (!(t == u)) {
      ++_asserts_failed_in_test;
      ++_asserts_failed;
      print(hint);
      print_nextline();
    }
  }

  static void assert_true(bool b, char const* hint) {
    assert_equal(b, true, hint);
  }

  template<class TestFunc>
  static void run_test(TestFunc test_func, char const* test_name) {
    ++_tests;
    _asserts_in_test = 0;
    _asserts_failed_in_test = 0;
    test_func();
    if (_asserts_in_test == 0) {
      ++_tests_skipped;
      print("[  SKIP  ] ");
      print(test_name);
      print_nextline();
    } else if (_asserts_failed_in_test == 0) {
      ++_tests_passed;
      print("[ PASSED ] ");
      print(test_name);
      print_nextline();
    } else {
      ++_tests_failed;
      print("[ FAILED ] ");
      print(test_name);
      print_nextline();
    }
  }

  static void print_result() {
    print_nextline();

    char str[64] = {0};
    snprintf(str,
             63,
             "Asserts: %d failed, %d passed",
             _asserts_failed,
             _asserts - _asserts_failed);
    print(str);
    print_nextline();

    memset(str, 0, 64);
    snprintf(str,
             63,
             "Tests:   %d failed, %d passed, %d skipped",
             _tests_failed,
             _tests_passed,
             _tests_skipped);
    print(str);
    print_nextline();

    if (_tests_failed == 0) {
      print("OK");
      print_nextline();
    } else {
      print("FAIL");
      print_nextline();
    }
  }

  static bool passed() {
    if (_tests_failed == 0) {
      return true;
    }
    return false;
  }
};

} // namespace emb

#define EMB_RUN_TEST(func) emb::test_runner::run_test(func, #func)

#if defined(EMBLIB_C28X)

#ifdef UNIT_TESTS_ENABLED
#define EMB_ASSERT_EQUAL(x, y)                                        \
  {                                                                   \
    const char* hint = "[  WARN  ] Assertion failed: " #x " != " #y   \
                       ", file: " __FILE__ ", line: " _STR(__LINE__); \
    emb::test_runner::assert_equal(x, y, hint);                       \
  }
#else
#define EMB_ASSERT_EQUAL(x, y) ((void)0)
#endif

#ifdef UNIT_TESTS_ENABLED
#define EMB_ASSERT_TRUE(x)                                                     \
  {                                                                            \
    const char* hint = "[  WARN  ] Assertion failed: " #x                      \
                       " is false, file: " __FILE__ ", line: " _STR(__LINE__); \
    emb::test_runner::assert_true(x, hint);                                    \
  }
#else
#define EMB_ASSERT_TRUE(x) ((void)0)
#endif

#elif defined(EMBLIB_ARM)

#ifdef UNIT_TESTS_ENABLED
#define EMB_ASSERT_EQUAL(x, y)                                                 \
  {                                                                            \
    const char* hint = "[  WARN  ] Assertion failed: " #x " != " #y            \
                       ", file: " __FILE__ ", line: " EMB_STRINGIZE(__LINE__); \
    emb::test_runner::assert_equal(x, y, hint);                                \
  }
#else
#define EMB_ASSERT_EQUAL(x, y) ((void)0)
#endif

#ifdef UNIT_TESTS_ENABLED
#define EMB_ASSERT_TRUE(x)                                              \
  {                                                                     \
    const char* hint =                                                  \
        "[  WARN  ] Assertion failed: " #x " is false, file: " __FILE__ \
        ", line: " EMB_STRINGIZE(__LINE__);                             \
    emb::test_runner::assert_true(x, hint);                             \
  }
#else
#define EMB_ASSERT_TRUE(x) ((void)0)
#endif

#endif
