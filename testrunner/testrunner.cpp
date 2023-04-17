#include "testrunner.h"


namespace emb {

void (*test_runner::print)(const char* str) = test_runner::print_dbg;
void (*test_runner::print_nextline)() = test_runner::print_nextline_dbg;

int test_runner::_asserts_in_test = 0;
int test_runner::_asserts = 0;
int test_runner::_asserts_failed_in_test = 0;
int test_runner::_asserts_failed = 0;
int test_runner::_tests = 0;
int test_runner::_tests_failed = 0;
int test_runner::_tests_passed = 0;
int test_runner::_tests_skipped = 0;

}

