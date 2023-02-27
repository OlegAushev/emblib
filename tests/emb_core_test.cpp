#include <c28x_emb/tests/emb_test.h>


class SingletonTest : public emb::c28x::InterruptInvoker<SingletonTest>
{
public:
	SingletonTest() : InterruptInvoker(this) {}
};


void EmbTest::CommonTest()
{
	SingletonTest singletonObject;
	SingletonTest* pSingletonObject = &singletonObject;
	EMB_ASSERT_EQUAL(SingletonTest::instance(), pSingletonObject);
}

