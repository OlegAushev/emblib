#include "testrunner.h"


namespace emb {

void (*TestRunner::print)(const char* str) = TestRunner::print_dbg;
void (*TestRunner::print_nextline)() = TestRunner::print_nextline_dbg;

int TestRunner::_asserts = 0;
int TestRunner::_asserts_failed_in_test = 0;
int TestRunner::_asserts_failed = 0;
int TestRunner::_tests = 0;
int TestRunner::_tests_failed = 0;
int TestRunner::_tests_passed = 0;

}

