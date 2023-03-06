#include <emblib_c28x/tests/tests.h>


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

