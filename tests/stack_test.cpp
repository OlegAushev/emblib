#include <emblib/tests/tests.h>


void emb::tests::stack_test() {
#ifdef EMB_TESTS_ENABLED
    emb::stack<int, 3> stack;

    EMB_ASSERT_TRUE(stack.empty());
    EMB_ASSERT_TRUE(!stack.full());

    stack.push(1);
    EMB_ASSERT_TRUE(!stack.empty());
    EMB_ASSERT_TRUE(!stack.full());
    EMB_ASSERT_EQUAL(stack.size(), 1);
    EMB_ASSERT_EQUAL(stack.top(), 1);

    stack.pop();
    EMB_ASSERT_TRUE(stack.empty());
    EMB_ASSERT_TRUE(!stack.full());

    stack.push(2);
    stack.push(3);
    stack.push(4);
    EMB_ASSERT_TRUE(stack.full());
    EMB_ASSERT_EQUAL(stack.size(), 3);
    EMB_ASSERT_EQUAL(stack.top(), 4);

    if (!stack.full()) {
        stack.push(5);
    }
    EMB_ASSERT_TRUE(stack.full());
    EMB_ASSERT_EQUAL(stack.size(), 3);
    EMB_ASSERT_EQUAL(stack.top(), 4);

    stack.pop();
    EMB_ASSERT_TRUE(!stack.empty());
    EMB_ASSERT_TRUE(!stack.full());
    EMB_ASSERT_EQUAL(stack.size(), 2);
    EMB_ASSERT_EQUAL(stack.top(), 3);

    stack.pop();
    EMB_ASSERT_TRUE(!stack.empty());
    EMB_ASSERT_TRUE(!stack.full());
    EMB_ASSERT_EQUAL(stack.size(), 1);
    EMB_ASSERT_EQUAL(stack.top(), 2);

    stack.pop();
    EMB_ASSERT_TRUE(stack.empty());

    stack.push(2);
    stack.push(3);
    stack.push(4);
    EMB_ASSERT_TRUE(stack.full());

    stack.clear();
    EMB_ASSERT_TRUE(stack.empty());
#endif
}

