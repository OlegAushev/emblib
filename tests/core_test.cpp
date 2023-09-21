#include <emblib_c28x/tests/tests.h>


class SingletonTest : public emb::interrupt_invoker<SingletonTest> {
public:
    SingletonTest() : interrupt_invoker(this) {}
};


void emb::tests::common_test() {
#ifdef EMB_TESTS_ENABLED
    SingletonTest singletonObject;
    SingletonTest* pSingletonObject = &singletonObject;
    EMB_ASSERT_EQUAL(SingletonTest::instance(), pSingletonObject);
#endif
}

