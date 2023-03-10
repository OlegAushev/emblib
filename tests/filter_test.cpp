#include <emblib_c28x/tests/tests.h>


void emb::tests::filter_test()
{
	/* MovingAvgFilter */
	emb::moving_avg_filter<int, 5> mvavg_filter_i16;
	for (size_t i = 1; i <= 5; ++i)
	{
		mvavg_filter_i16.update(i);
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
	emb::moving_avg_filter<float, 10> mvavg_filter_f32(filter_array);
	for (size_t i = 0; i < mvavg_filter_f32.size(); ++i)
	{
		mvavg_filter_f32.update(emb::numbers::pi * (1 + (i % 2)));
	}
	EMB_ASSERT_EQUAL(mvavg_filter_f32.output(), emb::numbers::pi * 1.5f);
	mvavg_filter_f32.set_output(emb::numbers::pi);
	EMB_ASSERT_EQUAL(mvavg_filter_f32.output(), emb::numbers::pi);

	/* MedianFilter */
	emb::median_filter<int, 5> med_filter;
	med_filter.update(-10);
	EMB_ASSERT_EQUAL(med_filter.output(), 0);
	med_filter.update(10);
	EMB_ASSERT_EQUAL(med_filter.output(), 0);
	med_filter.update(100);
	med_filter.update(100);
	med_filter.update(5);
	EMB_ASSERT_EQUAL(med_filter.output(), 10);
	med_filter.update(20);
	EMB_ASSERT_EQUAL(med_filter.output(), 20);
	med_filter.update(105);
	EMB_ASSERT_EQUAL(med_filter.output(), 100);
	med_filter.reset();
	EMB_ASSERT_EQUAL(med_filter.output(), 0);
	med_filter.set_output(50);
	EMB_ASSERT_EQUAL(med_filter.output(), 50);

	/* ExponentialMedianFilter */
	emb::exponential_median_filter<float, 3> expmed_filter;
	expmed_filter.set_smooth_factor(0.5);
	expmed_filter.update(16);
	EMB_ASSERT_EQUAL(expmed_filter.output(), 0);
	expmed_filter.update(8);
	EMB_ASSERT_EQUAL(expmed_filter.output(), 4);
	expmed_filter.update(32);
	EMB_ASSERT_EQUAL(expmed_filter.output(), 10);
	expmed_filter.update(8);
	EMB_ASSERT_EQUAL(expmed_filter.output(), 9);
	expmed_filter.set_smooth_factor(1);
	expmed_filter.update(19);
	EMB_ASSERT_EQUAL(expmed_filter.output(), 19);
	expmed_filter.set_output(10);
	EMB_ASSERT_EQUAL(expmed_filter.output(), 10);
	expmed_filter.update(5);
	expmed_filter.update(0);
	EMB_ASSERT_EQUAL(expmed_filter.output(), 5);
	expmed_filter.reset();
	EMB_ASSERT_EQUAL(expmed_filter.output(), 0);
}

