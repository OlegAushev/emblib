#include <c28x_emb/tests/emb_test.h>
#include <algorithm>
#include "float.h"


struct StructTest
{
	int key;
	int value;

	StructTest() {}
	StructTest(int k, int v) : key(k), value(v) {}
};

bool operator==(const StructTest& lhs, const StructTest& rhs)
{
	return (lhs.key == rhs.key) && (lhs.value == rhs.value);
}

bool operator<(const StructTest& lhs, const StructTest& rhs)
{
	return (lhs.key < rhs.key) || ((lhs.key == rhs.key) && (lhs.value < rhs.value));
}


void EmbTest::AlgorithmTest()
{
	// find
	int arr1[10] = {3, 6, 2, 7, 8, 9, 0, 1, 5, 4};
	EMB_ASSERT_EQUAL(*emb::find(arr1, arr1 + 10, 9), 9);
	EMB_ASSERT_EQUAL(emb::find(arr1, arr1 + 10, 7), arr1 + 3);
	EMB_ASSERT_EQUAL(emb::find(arr1, arr1 + 10, -1), arr1 + sizeof(arr1) / sizeof(arr1[0]));

	emb::Array<int, 5> arr2 = {-5, 1, 4, 4, 7};
	EMB_ASSERT_EQUAL(emb::find(arr2.begin(), arr2.end(), -5), arr2.begin());
	EMB_ASSERT_EQUAL(emb::find(arr2.begin(), arr2.end(), 5), arr2.end());
	EMB_ASSERT_EQUAL(emb::find(arr2.begin(), arr2.end(), 4), arr2.begin() + 2);

	emb::Array<StructTest, 5> arr3 = {StructTest(1, 2), StructTest(5, 0),
			StructTest(-4, 7), StructTest(8, 1), StructTest(5, -4)};
	EMB_ASSERT_EQUAL(emb::find(arr3.begin(), arr3.end(), StructTest(5, 0)), arr3.begin() + 1);
	EMB_ASSERT_EQUAL(emb::find(arr3.begin(), arr3.end(), StructTest(-4, 6)), arr3.end());
	EMB_ASSERT_EQUAL(*emb::find(arr3.begin(), arr3.end(), StructTest(-4, 7)), StructTest(-4, 7));

	// binary_find
	std::sort(arr1, arr1 + sizeof(arr1) / sizeof(arr1[0]));
	EMB_ASSERT_EQUAL(emb::binary_find(arr1, arr1 + sizeof(arr1) / sizeof(arr1[0]), 0), arr1);
	EMB_ASSERT_EQUAL(emb::binary_find(arr1, arr1 + sizeof(arr1) / sizeof(arr1[0]), 1), arr1 + 1);
	EMB_ASSERT_EQUAL(emb::binary_find(arr1, arr1 + sizeof(arr1) / sizeof(arr1[0]), 2), arr1 + 2);
	EMB_ASSERT_EQUAL(emb::binary_find(arr1, arr1 + sizeof(arr1) / sizeof(arr1[0]), 3), arr1 + 3);
	EMB_ASSERT_EQUAL(emb::binary_find(arr1, arr1 + sizeof(arr1) / sizeof(arr1[0]), 4), arr1 + 4);
	EMB_ASSERT_EQUAL(emb::binary_find(arr1, arr1 + sizeof(arr1) / sizeof(arr1[0]), 5), arr1 + 5);
	EMB_ASSERT_EQUAL(emb::binary_find(arr1, arr1 + sizeof(arr1) / sizeof(arr1[0]), 6), arr1 + 6);
	EMB_ASSERT_EQUAL(emb::binary_find(arr1, arr1 + sizeof(arr1) / sizeof(arr1[0]), 7), arr1 + 7);
	EMB_ASSERT_EQUAL(emb::binary_find(arr1, arr1 + sizeof(arr1) / sizeof(arr1[0]), 8), arr1 + 8);
	EMB_ASSERT_EQUAL(emb::binary_find(arr1, arr1 + sizeof(arr1) / sizeof(arr1[0]), 9), arr1 + 9);
	EMB_ASSERT_EQUAL(emb::binary_find(arr1, arr1 + sizeof(arr1) / sizeof(arr1[0]), -1), arr1 + 10);
	EMB_ASSERT_EQUAL(emb::binary_find(arr1, arr1 + sizeof(arr1) / sizeof(arr1[0]), 10), arr1 + 10);

	std::sort(arr2.begin(), arr2.begin());
	EMB_ASSERT_EQUAL(emb::binary_find(arr2.begin(), arr2.end(), -5), arr2.begin());
	EMB_ASSERT_EQUAL(emb::binary_find(arr2.begin(), arr2.end(), 1), arr2.begin() + 1);
	EMB_ASSERT_EQUAL(emb::binary_find(arr2.begin(), arr2.end(), 4), arr2.begin() + 2);
	EMB_ASSERT_EQUAL(emb::binary_find(arr2.begin(), arr2.end(), 7), arr2.begin() + 4);
	EMB_ASSERT_EQUAL(emb::binary_find(arr2.begin(), arr2.end(), -7), arr2.end());
	EMB_ASSERT_EQUAL(emb::binary_find(arr2.begin(), arr2.end(), 8), arr2.end());

	std::sort(arr3.begin(), arr3.end());
	EMB_ASSERT_EQUAL(emb::binary_find(arr3.begin(), arr3.end(), StructTest(-4, 7)), arr3.begin());
	EMB_ASSERT_EQUAL(emb::binary_find(arr3.begin(), arr3.end(), StructTest(1, 2)), arr3.begin() + 1);
	EMB_ASSERT_EQUAL(emb::binary_find(arr3.begin(), arr3.end(), StructTest(5, -4)), arr3.begin() + 2);
	EMB_ASSERT_EQUAL(emb::binary_find(arr3.begin(), arr3.end(), StructTest(5, 0)), arr3.begin() + 3);
	EMB_ASSERT_EQUAL(emb::binary_find(arr3.begin(), arr3.end(), StructTest(8, 1)), arr3.begin() + 4);
	EMB_ASSERT_EQUAL(emb::binary_find(arr3.begin(), arr3.end(), StructTest(-4, 6)), arr3.end());
	EMB_ASSERT_EQUAL(emb::binary_find(arr3.begin(), arr3.end(), StructTest(8, 0)), arr3.end());

	emb::Array<int, 9> arr4;
	emb::copy(arr1, arr1+9, arr4.data);
	EMB_ASSERT_EQUAL(emb::binary_find(arr4.begin(), arr4.end(), 0), arr4.begin());
	EMB_ASSERT_EQUAL(emb::binary_find(arr4.begin(), arr4.end(), 1), arr4.begin() + 1);
	EMB_ASSERT_EQUAL(emb::binary_find(arr4.begin(), arr4.end(), 2), arr4.begin() + 2);
	EMB_ASSERT_EQUAL(emb::binary_find(arr4.begin(), arr4.end(), 3), arr4.begin() + 3);
	EMB_ASSERT_EQUAL(emb::binary_find(arr4.begin(), arr4.end(), 4), arr4.begin() + 4);
	EMB_ASSERT_EQUAL(emb::binary_find(arr4.begin(), arr4.end(), 5), arr4.begin() + 5);
	EMB_ASSERT_EQUAL(emb::binary_find(arr4.begin(), arr4.end(), 6), arr4.begin() + 6);
	EMB_ASSERT_EQUAL(emb::binary_find(arr4.begin(), arr4.end(), 7), arr4.begin() + 7);
	EMB_ASSERT_EQUAL(emb::binary_find(arr4.begin(), arr4.end(), 8), arr4.begin() + 8);
	EMB_ASSERT_EQUAL(emb::binary_find(arr4.begin(), arr4.end(), 9), arr4.end());
	EMB_ASSERT_EQUAL(emb::binary_find(arr4.begin(), arr4.end(), -1), arr4.end());

	// fill, count
	emb::Array<int, 10> arr5;
	arr5.fill(0);
	EMB_ASSERT_EQUAL(emb::count(arr5.begin(), arr5.end(), 0), 10);
	arr5[5] = 5;
	EMB_ASSERT_EQUAL(emb::count(arr5.begin(), arr5.end(), 0), 9);
	EMB_ASSERT_EQUAL(emb::count(arr5.begin(), arr5.end(), 5), 1);
	EMB_ASSERT_EQUAL(emb::count(arr5.begin(), arr5.end(), 1), 0);
	arr5[7] = 5;
	EMB_ASSERT_EQUAL(emb::count(arr5.begin(), arr5.end(), 5), 2);
	emb::fill(arr5.begin(), arr5.end(), 1);
	EMB_ASSERT_EQUAL(emb::count(arr5.begin(), arr5.end(), 0), 0);
	EMB_ASSERT_EQUAL(emb::count(arr5.begin(), arr5.end(), 5), 0);
	EMB_ASSERT_EQUAL(emb::count(arr5.begin(), arr5.end(), 1), 10);

	// copy, equal
	int arr6[5] = {-2, 1, 0, 1, 2};
	emb::Array<int, 5> arr7;
	arr7.fill(0);
	emb::copy(arr6, arr6 + 5, arr7.begin());
	EMB_ASSERT_TRUE(emb::equal(arr7.begin(), arr7.end(), arr6));
	for (size_t i = 0; i < arr7.size(); ++i)
	{
		EMB_ASSERT_EQUAL(arr6[i], arr7[i]);
	}

	arr6[3] = 0;
	EMB_ASSERT_TRUE(!emb::equal(arr7.begin(), arr7.end(), arr6));
	arr7[3] = 0;
	EMB_ASSERT_TRUE(emb::equal(arr7.begin(), arr7.end(), arr6));

	int arr8[5] = {0};
	emb::copy(arr6, arr6 + 5, arr8);
	EMB_ASSERT_TRUE(emb::equal(arr7.begin(), arr7.end(), arr8));

	// copy, equal, count, fill
	emb::Array<StructTest, 5> arr9;
	arr9[0] = StructTest(1, 2);
	arr9[1] = StructTest(8, 1);
	arr9[2] = StructTest(-4, 7);
	arr9[3] = StructTest(8, 1);
	arr9[4] = StructTest(5, -4);
	emb::Array<StructTest, 5> arr10;
	emb::copy(arr9.begin(), arr9.end(), arr10.begin());
	EMB_ASSERT_TRUE(emb::equal(arr10.begin(), arr10.end(), arr9.begin()));
	EMB_ASSERT_EQUAL(emb::count(arr10.begin(), arr10.end(), StructTest(-4, 7)), 1);
	EMB_ASSERT_EQUAL(emb::count(arr10.begin(), arr10.end(), StructTest(8, 1)), 2);
	EMB_ASSERT_EQUAL(emb::count(arr10.begin(), arr10.end(), StructTest(8, 2)), 0);
	emb::fill(arr9.begin(), arr9.end(), StructTest(42, 314));
	arr10.fill(StructTest(42, 314));
	EMB_ASSERT_TRUE(emb::equal(arr9.begin(), arr9.end(), arr10.begin()));
	EMB_ASSERT_EQUAL(emb::count(arr9.begin(), arr9.end(), StructTest(42, 314)), 5);

	// clamp
	EMB_ASSERT_EQUAL(emb::clamp(-1, -10, 5), -1);
	EMB_ASSERT_EQUAL(emb::clamp(-10, -4, 5), -4);
	EMB_ASSERT_EQUAL(emb::clamp(-10, -40, -15), -15);
	EMB_ASSERT_EQUAL(emb::clamp(2.1f, 1.1f, 5.5f), 2.1f);
	EMB_ASSERT_EQUAL(emb::clamp(-2.1f, -1.1f, 5.5f), -1.1f);
	EMB_ASSERT_EQUAL(emb::clamp(7.1f, -10.1f, -5.5f), -5.5f);
	EMB_ASSERT_EQUAL(emb::clamp(-70.1f, -10.1f, -5.5f), -10.1f);
	EMB_ASSERT_EQUAL(emb::clamp(-7.1f, -10.1f, -5.5f), -7.1f);
	EMB_ASSERT_EQUAL(emb::clamp(float(INFINITY), -FLT_MAX, FLT_MAX), FLT_MAX);
	EMB_ASSERT_EQUAL(emb::clamp(float(-INFINITY), -FLT_MAX, FLT_MAX), -FLT_MAX);
	EMB_ASSERT_EQUAL(emb::clamp(-314.16f, -FLT_MAX, FLT_MAX), -314.16f);
	EMB_ASSERT_EQUAL(emb::clamp(-0.1f, 0.f, 1.f), 0.f);
	EMB_ASSERT_EQUAL(emb::clamp(0.f, 0.f, 1.f), 0.f);
	EMB_ASSERT_EQUAL(emb::clamp(0.5f, 0.f, 1.f), 0.5f);
	EMB_ASSERT_EQUAL(emb::clamp(1.f, 0.f, 1.f), 1.f);
	EMB_ASSERT_EQUAL(emb::clamp(1.1f, 0.f, 1.f), 1.f);

	// minmax
	emb::Array<int, 10> arr11 = {3, 6, 2, 7, 8, 9, 0, 1, 5, 4};
	EMB_ASSERT_EQUAL(*(emb::max_element(arr11.begin(), arr11.end())), 9);
	EMB_ASSERT_EQUAL(*(emb::min_element(arr11.begin(), arr11.end())), 0);
	EMB_ASSERT_EQUAL(*(emb::minmax_element(arr11.begin(), arr11.end()).first), 0);
	EMB_ASSERT_EQUAL(*(emb::minmax_element(arr11.begin(), arr11.end()).second), 9);
	emb::Array<StructTest, 5> arr12 = {StructTest(1, 2), StructTest(8, 1), StructTest(-4, 7), StructTest(8, 1), StructTest(5, -4)};
	EMB_ASSERT_EQUAL(*(emb::max_element(arr12.begin(), arr12.end())), StructTest(8, 1));
	EMB_ASSERT_EQUAL(*(emb::min_element(arr12.begin(), arr12.end())), StructTest(-4, 7));
	EMB_ASSERT_EQUAL(*(emb::minmax_element(arr12.begin(), arr12.end()).first), StructTest(-4, 7));
	EMB_ASSERT_EQUAL(*(emb::minmax_element(arr12.begin(), arr12.end()).second), StructTest(8, 1));
}

