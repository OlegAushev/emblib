#include <emblib_c28x/tests/tests.h>


void emb::tests::circular_buffer_test()
{
	emb::circular_buffer<int, 4> buf;
	EMB_ASSERT_TRUE(buf.empty());

	buf.push(1);
	EMB_ASSERT_EQUAL(buf.front(), 1);
	EMB_ASSERT_EQUAL(buf.back(), 1);
	EMB_ASSERT_EQUAL(buf.size(), 1);
	buf.pop();
	EMB_ASSERT_TRUE(buf.empty());
	EMB_ASSERT_EQUAL(buf.size(), 0);
	if (!buf.empty()) buf.pop();
	EMB_ASSERT_TRUE(buf.empty());

	buf.push(2);
	EMB_ASSERT_EQUAL(buf.front(), 2);
	EMB_ASSERT_EQUAL(buf.back(), 2);
	EMB_ASSERT_EQUAL(buf.size(), 1);
	buf.push(3);
	EMB_ASSERT_EQUAL(buf.size(), 2);
	EMB_ASSERT_EQUAL(buf.back(), 3);
	buf.push(4);
	EMB_ASSERT_EQUAL(buf.size(), 3);
	EMB_ASSERT_TRUE(!buf.full());
	EMB_ASSERT_EQUAL(buf.back(), 4);
	buf.push(5);
	EMB_ASSERT_EQUAL(buf.size(), 4);
	EMB_ASSERT_TRUE(buf.full());
	EMB_ASSERT_EQUAL(buf.back(), 5);
	buf.push(6);
	EMB_ASSERT_EQUAL(buf.size(), 4);
	EMB_ASSERT_EQUAL(buf.front(), 3);
	EMB_ASSERT_EQUAL(buf.back(), 6);
	buf.push(7);
	buf.push(8);
	EMB_ASSERT_EQUAL(buf.front(), 5);
	EMB_ASSERT_EQUAL(buf.back(), 8);
	buf.push(9);
	EMB_ASSERT_EQUAL(buf.front(), 6);
	EMB_ASSERT_EQUAL(buf.back(), 9);

	buf.pop();
	EMB_ASSERT_EQUAL(buf.front(), 7);
	EMB_ASSERT_EQUAL(buf.back(), 9);
	buf.pop();
	EMB_ASSERT_EQUAL(buf.back(), 9);
	EMB_ASSERT_EQUAL(buf.size(), 2);
	EMB_ASSERT_EQUAL(buf.front(), 8);

	buf.push(10);
	EMB_ASSERT_EQUAL(buf.size(), 3);
	EMB_ASSERT_EQUAL(buf.front(), 8);
	EMB_ASSERT_EQUAL(buf.back(), 10);

	buf.push(11);
	EMB_ASSERT_TRUE(buf.full());
	EMB_ASSERT_EQUAL(buf.front(), 8);
	EMB_ASSERT_EQUAL(buf.back(), 11);
}

