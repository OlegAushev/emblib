#include <emblib/tests/tests.hpp>

void emb::tests::queue_test() {
#ifdef EMB_TESTS_ENABLED
  emb::queue<int, 10> queue;
  EMB_ASSERT_TRUE(queue.empty());

  queue.push(1);
  EMB_ASSERT_TRUE(!queue.empty());
  EMB_ASSERT_TRUE(!queue.full());

  EMB_ASSERT_EQUAL(queue.front(), 1);
  EMB_ASSERT_EQUAL(queue.back(), 1);
  queue.pop();
  EMB_ASSERT_TRUE(queue.empty());
  EMB_ASSERT_TRUE(!queue.full());

  if (!queue.empty())
    queue.pop();
  if (!queue.empty())
    queue.pop();
  EMB_ASSERT_TRUE(queue.empty());
  EMB_ASSERT_TRUE(!queue.full());

  for (int i = 1; i <= int(queue.capacity()); ++i) {
    queue.push(i);
    EMB_ASSERT_EQUAL(queue.back(), i);
  }
  EMB_ASSERT_TRUE(queue.full());
  if (!queue.full()) {
    queue.push(11);
  }
  EMB_ASSERT_EQUAL(queue.front(), 1);
  EMB_ASSERT_EQUAL(queue.back(), queue.capacity());
  queue.pop();
  EMB_ASSERT_TRUE(!queue.full());
  queue.push(11);
  EMB_ASSERT_EQUAL(queue.back(), 11);
  EMB_ASSERT_EQUAL(queue.front(), 2);

  queue.clear();
  for (int i = 1; i <= int(queue.capacity()); ++i) {
    queue.push(i);
  }
  for (int i = 0; i < int(queue.capacity() / 2); ++i) {
    queue.pop();
  }
  EMB_ASSERT_EQUAL(queue.size(), 5);
  EMB_ASSERT_EQUAL(queue.front(), 6);
  EMB_ASSERT_EQUAL(queue.back(), queue.capacity());
  for (int i = 0; i < int(queue.capacity() / 2); ++i) {
    queue.pop();
  }
  EMB_ASSERT_EQUAL(queue.size(), 0);
  EMB_ASSERT_TRUE(queue.empty());

  queue.clear();
  for (int i = 1; i <= int(queue.capacity()) + 5; ++i) {
    if (!queue.full())
      queue.push(i);
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
  for (int i = 1; i <= int(queue.capacity()) + 5; ++i) {
    if (!queue.full()) {
      queue.push(i);
    }
  }
  for (int i = 1; i <= int(queue.capacity()); ++i) {
    EMB_ASSERT_EQUAL(queue.front(), i);
    queue.pop();
  }
  EMB_ASSERT_TRUE(queue.empty());
#endif
}
