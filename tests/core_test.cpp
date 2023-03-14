#include <emblib_c28x/tests/tests.h>


class SingletonTest : public emb::c28x::interrupt_invoker<SingletonTest> {
public:
	SingletonTest() : interrupt_invoker(this) {}
};


void emb::tests::common_test() {
	SingletonTest singletonObject;
	SingletonTest* pSingletonObject = &singletonObject;
	EMB_ASSERT_EQUAL(SingletonTest::instance(), pSingletonObject);
}

