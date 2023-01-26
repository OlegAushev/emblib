#include <emb_c28x/tests/emb_test.h>


void EmbTest::QueueTest()
{
	emb::Queue<int, 10> queue;
	EMB_ASSERT_TRUE(queue.empty());

	queue.push(1);
	EMB_ASSERT_TRUE(!queue.empty());
	EMB_ASSERT_TRUE(!queue.full());

	EMB_ASSERT_EQUAL(queue.front(), 1);
	EMB_ASSERT_EQUAL(queue.back(), 1);
	queue.pop();
	EMB_ASSERT_TRUE(queue.empty());
	EMB_ASSERT_TRUE(!queue.full());

	if (!queue.empty()) queue.pop();
	if (!queue.empty()) queue.pop();
	EMB_ASSERT_TRUE(queue.empty());
	EMB_ASSERT_TRUE(!queue.full());

	for (size_t i = 1; i <= queue.capacity(); ++i)
	{
		queue.push(i);
		EMB_ASSERT_EQUAL(queue.back(), i);
	}
	EMB_ASSERT_TRUE(queue.full());
	if (!queue.full()) queue.push(11);
	EMB_ASSERT_EQUAL(queue.front(), 1);
	EMB_ASSERT_EQUAL(queue.back(), queue.capacity());
	queue.pop();
	EMB_ASSERT_TRUE(!queue.full());
	queue.push(11);
	EMB_ASSERT_EQUAL(queue.back(), 11);
	EMB_ASSERT_EQUAL(queue.front(), 2);


	queue.clear();
	for (size_t i = 1; i <= queue.capacity(); ++i)
	{
		queue.push(i);
	}
	for (size_t i = 0; i < queue.capacity()/2; ++i)
	{
		queue.pop();
	}
	EMB_ASSERT_EQUAL(queue.size(), 5);
	EMB_ASSERT_EQUAL(queue.front(), 6);
	EMB_ASSERT_EQUAL(queue.back(), queue.capacity());
	for (size_t i = 0; i < queue.capacity()/2; ++i)
	{
		queue.pop();
	}
	EMB_ASSERT_EQUAL(queue.size(), 0);
	EMB_ASSERT_TRUE(queue.empty());

	queue.clear();
	for (size_t i = 1; i <= queue.capacity() + 5; ++i)
	{
		if (!queue.full()) queue.push(i);
	}
	EMB_ASSERT_EQUAL(queue.size(), 10);
	EMB_ASSERT_TRUE(queue.full());
	queue.pop();
	EMB_ASSERT_EQUAL(queue.size(), 9);
	EMB_ASSERT_TRUE(!queue.full());
	queue.pop();
	EMB_ASSERT_EQUAL(queue.size(), 8);
	EMB_ASSERT_TRUE(!queue.full());
	EMB_ASSERT_EQUAL(queue.front(), 3);

	queue.clear();
	for (size_t i = 1; i <= queue.capacity() + 5; ++i)
	{
		if (!queue.full()) queue.push(i);
	}
	for (size_t i = 1; i <= queue.capacity(); ++i)
	{
		EMB_ASSERT_EQUAL(queue.front(), i);
		queue.pop();
	}
	EMB_ASSERT_TRUE(queue.empty());
}

