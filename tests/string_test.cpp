#include <emblib_c28x/tests/tests.h>


void emb::tests::string_test() {
	emb::string<16> str1 = "Hello, world!";
	EMB_ASSERT_EQUAL(str1.lenght(), 13);
	EMB_ASSERT_EQUAL(strlen(str1.data()), 13);
	EMB_ASSERT_EQUAL(str1.front(), 'H');
	EMB_ASSERT_EQUAL(str1.back(), '!');

	str1.pop_back();
	EMB_ASSERT_EQUAL(strlen(str1.data()), 12);
	EMB_ASSERT_EQUAL(str1.back(), 'd');

	str1.push_back('?');
	EMB_ASSERT_EQUAL(str1[12], '?');
	EMB_ASSERT_EQUAL(str1.back(), '?');
	EMB_ASSERT_EQUAL(str1.lenght(), 13);
	EMB_ASSERT_EQUAL(strlen(str1.data()), 13);

	str1.insert(0, '-');
	EMB_ASSERT_EQUAL(str1.front(), '-');
	EMB_ASSERT_EQUAL(strlen(str1.data()), 14);

	str1.insert(2, 'i');
	EMB_ASSERT_EQUAL(str1[2], 'i');

	str1.insert(str1.lenght(), '@');
	EMB_ASSERT_EQUAL(strcmp(str1.data(), "-Hiello, world?@"), 0);

	str1.clear();
	EMB_ASSERT_EQUAL(str1.lenght(), 0);
	EMB_ASSERT_EQUAL(strlen(str1.begin()), 0);
}

