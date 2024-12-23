#include <emblib/tests/tests.hpp>


void emb::tests::filter_test() {
#ifdef EMB_TESTS_ENABLED
    /* MovingAvgFilter */
    emb::movavg_filter<int, 5> mvavg_filter_i16;
    for (int i = 1; i <= 5; ++i) {
        mvavg_filter_i16.push(i);
    }
    EMB_ASSERT_EQUAL(mvavg_filter_i16.output(), 3);
    mvavg_filter_i16.reset();
    EMB_ASSERT_EQUAL(mvavg_filter_i16.output(), 0);
    EMB_ASSERT_EQUAL(mvavg_filter_i16.size(), 5);
    mvavg_filter_i16.resize(3);
    EMB_ASSERT_EQUAL(mvavg_filter_i16.size(), 3);
    mvavg_filter_i16.resize(6);
    EMB_ASSERT_EQUAL(mvavg_filter_i16.size(), 5);

    emb::array<float, 10> filter_array;
    emb::movavg_filter<float, 10> mvavg_filter_f32(filter_array);
    for (int i = 0; i < int(mvavg_filter_f32.size()); ++i) {
        mvavg_filter_f32.push(emb::numbers::pi * float(1 + (i % 2)));
    }
    EMB_ASSERT_EQUAL(mvavg_filter_f32.output(), emb::numbers::pi * 1.5f);
    mvavg_filter_f32.set_output(emb::numbers::pi);
    EMB_ASSERT_EQUAL(mvavg_filter_f32.output(), emb::numbers::pi);

    /* MedianFilter */
    emb::med_filter<int, 5> med_filter_1;
    med_filter_1.push(-10);
    EMB_ASSERT_EQUAL(med_filter_1.output(), 0);
    med_filter_1.push(10);
    EMB_ASSERT_EQUAL(med_filter_1.output(), 0);
    med_filter_1.push(100);
    med_filter_1.push(100);
    med_filter_1.push(5);
    EMB_ASSERT_EQUAL(med_filter_1.output(), 10);
    med_filter_1.push(20);
    EMB_ASSERT_EQUAL(med_filter_1.output(), 20);
    med_filter_1.push(105);
    EMB_ASSERT_EQUAL(med_filter_1.output(), 100);
    med_filter_1.reset();
    EMB_ASSERT_EQUAL(med_filter_1.output(), 0);
    med_filter_1.set_output(50);
    EMB_ASSERT_EQUAL(med_filter_1.output(), 50);

    /* ExponentialMedianFilter */
    emb::expmed_filter<float, 3> expmed_filter_1;
    expmed_filter_1.init(0.5, 1);
    expmed_filter_1.push(16);
    EMB_ASSERT_EQUAL(expmed_filter_1.output(), 0);
    expmed_filter_1.push(8);
    EMB_ASSERT_EQUAL(expmed_filter_1.output(), 4);
    expmed_filter_1.push(32);
    EMB_ASSERT_EQUAL(expmed_filter_1.output(), 10);
    expmed_filter_1.push(8);
    EMB_ASSERT_EQUAL(expmed_filter_1.output(), 9);
    expmed_filter_1.init(1, 1);
    expmed_filter_1.push(19);
    EMB_ASSERT_EQUAL(expmed_filter_1.output(), 19);
    expmed_filter_1.set_output(10);
    EMB_ASSERT_EQUAL(expmed_filter_1.output(), 10);
    expmed_filter_1.push(5);
    expmed_filter_1.push(0);
    EMB_ASSERT_EQUAL(expmed_filter_1.output(), 5);
    expmed_filter_1.reset();
    EMB_ASSERT_EQUAL(expmed_filter_1.output(), 0);
#endif
}

