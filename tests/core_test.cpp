#include <emblib/tests/tests.hpp>


class SingletonTest : public emb::singleton<SingletonTest> {
public:
    SingletonTest() : singleton(this) {}
};


void emb::tests::common_test() {
#ifdef EMB_TESTS_ENABLED
    SingletonTest singletonObject;
    EMB_MAYBE_UNUSED SingletonTest* pSingletonObject = &singletonObject;
    EMB_ASSERT_EQUAL(SingletonTest::instance(), pSingletonObject);
#endif
}

