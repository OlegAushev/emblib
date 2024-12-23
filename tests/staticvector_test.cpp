#include <emblib/tests/tests.hpp>


void emb::tests::static_vector_test() {
#ifdef EMB_TESTS_ENABLED
    emb::static_vector<int, 5> vec1;
    EMB_ASSERT_EQUAL(vec1.size(), 0);
    EMB_ASSERT_EQUAL(vec1.capacity(), 5);
    vec1.resize(3);
    EMB_ASSERT_EQUAL(vec1[2], 0);
    vec1.clear();
    vec1.resize(3, 1);
    EMB_ASSERT_EQUAL(vec1[2], 1);

    vec1.push_back(2);
    EMB_ASSERT_EQUAL(vec1.size(), 4);
    EMB_ASSERT_EQUAL(vec1.back(), 2);

    vec1.insert(vec1.begin(), 3);
    EMB_ASSERT_EQUAL(vec1.size(), 5);
    EMB_ASSERT_EQUAL(vec1.front(), 3);
    EMB_ASSERT_EQUAL(vec1.back(), 2);

    vec1.erase(vec1.begin()+1, vec1.begin()+4);
    EMB_ASSERT_EQUAL(vec1.size(), 2);
    EMB_ASSERT_EQUAL(vec1[0], 3);
    EMB_ASSERT_EQUAL(vec1[1], 2);

    emb::static_vector<int, 7> vec2(vec1.begin(), vec1.end());
    EMB_ASSERT_EQUAL(vec2.size(), 2);
    vec2.insert(vec2.begin(), vec1.begin(), vec1.end());
    EMB_ASSERT_EQUAL(vec2.size(), 4);
    vec2.insert(vec2.end(), 3, -1);
    EMB_ASSERT_TRUE(vec2.full());

    vec2.erase(vec2.begin());
    vec2.erase(vec2.end()-1);
    EMB_ASSERT_EQUAL(vec2.size(), 5);
    EMB_ASSERT_EQUAL(vec2[0], 2);
    EMB_ASSERT_EQUAL(vec2[1], 3);
    EMB_ASSERT_EQUAL(vec2[2], 2);
    EMB_ASSERT_EQUAL(vec2[3], -1);
    EMB_ASSERT_EQUAL(vec2[4], -1);
    vec2.erase(vec2.begin(), vec2.begin()+1);
    vec2.erase(vec2.end()-2, vec2.end());
    EMB_ASSERT_EQUAL(vec2.size(), 2);
    EMB_ASSERT_EQUAL(vec2[0], 3);
    EMB_ASSERT_EQUAL(vec2[1], 2);
#endif
}

