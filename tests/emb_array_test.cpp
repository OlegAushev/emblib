#include <emb_c28x/tests/emb_test.h>


void EmbTest::ArrayTest()
{
	emb::Array<float, 5> arr1 = {0, 1, 2, 3, 4};
	EMB_ASSERT_EQUAL(arr1[0], 0);
	EMB_ASSERT_EQUAL(arr1[1], 1);
	EMB_ASSERT_EQUAL(arr1[2], 2);
	EMB_ASSERT_EQUAL(arr1[3], 3);
	EMB_ASSERT_EQUAL(arr1[4], 4);

	emb::Array<float, 5> arr2;
	std::copy(arr1.begin(), arr1.end(), arr2.begin());
	for (size_t i = 0; i < arr2.size(); ++i)
	{
		EMB_ASSERT_EQUAL(arr1.at(i), arr2.at(i));
	}

	arr2.fill(5);
	for (size_t i = 0; i < arr2.size(); ++i)
	{
		EMB_ASSERT_EQUAL(arr2[i], 5);
	}

	emb::Array<float, 5>arr3 = {0};
	for (size_t i = 0; i < arr3.size(); ++i)
	{
		EMB_ASSERT_EQUAL(arr3[i], 0);
	}
}

