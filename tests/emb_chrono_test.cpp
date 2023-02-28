#include "emb_tests.h"


void emb::tests::chrono()
{
	emb::chrono::seconds sec(10);
	EMB_ASSERT_EQUAL(sec.count(), 10);
	emb::chrono::milliseconds msec = emb::chrono::duration_cast<emb::chrono::milliseconds>(sec);
	EMB_ASSERT_EQUAL(msec.count(), 10000);
	EMB_ASSERT_EQUAL(msec, emb::chrono::milliseconds(10000));

	emb::chrono::microseconds usec = emb::chrono::duration_cast<emb::chrono::microseconds>(sec);
	EMB_ASSERT_EQUAL(usec.count(), 10000000);

	emb::chrono::nanoseconds nsec = emb::chrono::duration_cast<emb::chrono::nanoseconds>(usec);
	EMB_ASSERT_EQUAL(nsec.count(), 10000000000);

	EMB_ASSERT_EQUAL(msec - emb::chrono::milliseconds(1), emb::chrono::milliseconds(9999));

	emb::chrono::milliseconds msec2 = msec + emb::chrono::milliseconds(1);
	EMB_ASSERT_EQUAL(msec2.count(), 10001);

	EMB_ASSERT_TRUE(msec2 > msec);
	EMB_ASSERT_TRUE(emb::chrono::microseconds(1) < emb::chrono::microseconds(2));
}

