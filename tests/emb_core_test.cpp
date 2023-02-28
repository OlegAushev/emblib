#include "emb_tests.h"


class SingletonTest : public emb::c28x::InterruptInvoker<SingletonTest>
{
public:
	SingletonTest() : InterruptInvoker(this) {}
};


void emb::tests::common()
{
	SingletonTest singletonObject;
	SingletonTest* pSingletonObject = &singletonObject;
	EMB_ASSERT_EQUAL(SingletonTest::instance(), pSingletonObject);
}

