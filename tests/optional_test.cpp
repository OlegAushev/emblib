#include <emblib/tests/tests.h>


void emb::tests::optional_test() {
    emb::optional<int> opt1;
    EMB_ASSERT_TRUE(!opt1.has_value());

    emb::optional<int> opt2;
    opt2 = opt1;
    EMB_ASSERT_TRUE(!opt2.has_value());

    opt1 = 42;
    EMB_ASSERT_TRUE(opt1.has_value());
    EMB_ASSERT_EQUAL(opt1.value(), 42);

    opt2 = opt1;
    EMB_ASSERT_TRUE(opt2.has_value());
    EMB_ASSERT_EQUAL(opt2.value(), 42);

    emb::optional<int> opt3 = opt2;
    EMB_ASSERT_TRUE(opt3.has_value());
    EMB_ASSERT_EQUAL(opt3.value(), 42);
    EMB_ASSERT_EQUAL(opt3.value_or(43), 42);
    opt3.reset();
    EMB_ASSERT_TRUE(!opt3.has_value());
    EMB_ASSERT_EQUAL(opt3.value_or(43), 43);
}
