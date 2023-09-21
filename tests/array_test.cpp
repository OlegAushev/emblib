#include <emblib/tests/tests.h>


void emb::tests::array_test() {
#ifdef EMB_TESTS_ENABLED
    emb::array<float, 5> arr1 = {0, 1, 2, 3, 4};
    EMB_ASSERT_EQUAL(arr1[0], 0);
    EMB_ASSERT_EQUAL(arr1[1], 1);
    EMB_ASSERT_EQUAL(arr1[2], 2);
    EMB_ASSERT_EQUAL(arr1[3], 3);
    EMB_ASSERT_EQUAL(arr1[4], 4);

    emb::array<float, 5> arr2;
    std::copy(arr1.begin(), arr1.end(), arr2.begin());
    for (int i = 0; i < arr2.size(); ++i) {
        EMB_ASSERT_EQUAL(arr1.at(i), arr2.at(i));
    }

    arr2.fill(5);
    for (int i = 0; i < arr2.size(); ++i) {
        EMB_ASSERT_EQUAL(arr2[i], 5);
    }

    emb::array<float, 5>arr3 = {0};
    for (int i = 0; i < arr3.size(); ++i) {
        EMB_ASSERT_EQUAL(arr3[i], 0);
    }
#endif
}

