#include <emblib/tests/tests.h>


struct Foo {
    static int counter;
    int id;
    Foo() : id(++counter) {}
    Foo(const Foo& other) : id(other.id) { ++counter; }
    ~Foo() { --counter; }
};
int Foo::counter = 0;


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

    emb::optional<int> opt4 = emb::nullopt;
    EMB_ASSERT_TRUE(!opt4.has_value());
    opt4 = 44;
    EMB_ASSERT_TRUE(opt4.has_value());
    EMB_ASSERT_EQUAL(opt4.value(), 44);


    emb::optional<Foo> fopt1 = Foo();
    EMB_ASSERT_EQUAL(fopt1.value().id, 1);
    EMB_ASSERT_EQUAL(Foo::counter, 1);

    emb::optional<Foo> fopt2;
    fopt2 = fopt1.value();
    EMB_ASSERT_EQUAL(fopt2.value().id, 1);
    EMB_ASSERT_EQUAL(Foo::counter, 2);

    fopt2 = Foo();
    EMB_ASSERT_EQUAL(fopt2.value().id, 3);
    EMB_ASSERT_EQUAL(Foo::counter, 2);
}
