#include <emblib/tests/tests.h>


class SingletonTest : public emb::interrupt_invoker<SingletonTest> {
public:
    SingletonTest() : interrupt_invoker(this) {}
};


void emb::tests::common_test() {
#ifdef EMB_TESTS_ENABLED
    SingletonTest singletonObject;
    EMB_MAYBE_UNUSED SingletonTest* pSingletonObject = &singletonObject;
    EMB_ASSERT_EQUAL(SingletonTest::instance(), pSingletonObject);
#endif
}

