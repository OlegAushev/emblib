#include <c28x_emb/tests/emb_test.h>


class SingletonTest : public emb::c28x::interrupt_invoker<SingletonTest>
{
public:
	SingletonTest() : interrupt_invoker(this) {}
};


void EmbTest::CommonTest()
{
	SingletonTest singletonObject;
	SingletonTest* pSingletonObject = &singletonObject;
	EMB_ASSERT_EQUAL(SingletonTest::instance(), pSingletonObject);
}

