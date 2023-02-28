#include "emb_tests.h"


void emb::tests::bitset()
{
	// sizeof
	emb::Bitset<1> bs;
	EMB_ASSERT_EQUAL(sizeof(bs), 1);
	emb::Bitset<16> bs16;
	EMB_ASSERT_EQUAL(sizeof(bs16), 1);

	emb::Bitset<17> bs17;
	EMB_ASSERT_EQUAL(sizeof(bs17), 2);
	emb::Bitset<32> bs32;
	EMB_ASSERT_EQUAL(sizeof(bs32), 2);

	emb::Bitset<33> bs33;
	EMB_ASSERT_EQUAL(sizeof(bs33), 3);
	emb::Bitset<48> bs48;
	EMB_ASSERT_EQUAL(sizeof(bs48), 3);

	// ctor, operator[]
	emb::Array<unsigned int, 2> arr1 = {0xAAAA, 0xFF00};
	emb::Bitset<32> bs1(arr1);

	for (size_t i = 0; i < 15; ++i)
	{
		EMB_ASSERT_EQUAL(bs1[i], ((i%2) != 0));
	}

	for (size_t i = 16; i < 24; ++i)
	{
		EMB_ASSERT_EQUAL(bs1[i], false);
	}

	for (size_t i = 24; i < 32; ++i)
	{
		EMB_ASSERT_EQUAL(bs1[i], true);
	}

	uint64_t u64 = 0x8888888888888888;
	emb::Bitset<64> bs2(u64);
	for (size_t i = 0; i < 64; ++i)
	{
		EMB_ASSERT_EQUAL(bs2[i], (((i+1)%4) == 0));
	}

	uint16_t u16 = 0x0001;
	emb::Bitset<32> bs3(u16);
	EMB_ASSERT_TRUE(bs3[0]);
	for (size_t i = 1; i < 32; ++i)
	{
		EMB_ASSERT_TRUE(!bs3[i]);
	}

	// all, any, none, count
	EMB_ASSERT_TRUE(emb::Bitset<16>(0xFFFF).all());
	EMB_ASSERT_TRUE(!emb::Bitset<16>(0xFFCF).all());
	EMB_ASSERT_TRUE(emb::Bitset<32>(0xFFFFFFFF).all());
	EMB_ASSERT_TRUE(!emb::Bitset<32>(0xFFEFFFFF).all());
	EMB_ASSERT_EQUAL(emb::Bitset<16>(0xFFFF).count(), 16);
	EMB_ASSERT_EQUAL(emb::Bitset<16>(0xFAFF).count(), 14);
	EMB_ASSERT_EQUAL(emb::Bitset<32>(0xFFFFFFFF).count(), 32);
	EMB_ASSERT_EQUAL(emb::Bitset<32>(0xFFEFFFFF).count(), 31);

	EMB_ASSERT_TRUE(emb::Bitset<16>(0x00D0).any());
	EMB_ASSERT_TRUE(!emb::Bitset<16>(0x0000).any());
	EMB_ASSERT_TRUE(emb::Bitset<64>(0x0100000000000000).any());
	EMB_ASSERT_TRUE(!emb::Bitset<64>(0x0000000000000000).any());
	EMB_ASSERT_EQUAL(emb::Bitset<64>(0x0100000000000000).count(), 1);
	EMB_ASSERT_EQUAL(emb::Bitset<64>(0x0000000000000000).count(), 0);

	EMB_ASSERT_TRUE(emb::Bitset<64>(0x0000000000000000).none());
	EMB_ASSERT_TRUE(!emb::Bitset<64>(0x0000100000000000).none());

	// flip
	bs16.set(0); bs16.set(4); bs16.set(8); bs16.set(12);
	bs16.flip();
	EMB_ASSERT_TRUE(!bs16[8]);
	EMB_ASSERT_EQUAL(bs16.count(), 12);
	bs16.flip(8);
	EMB_ASSERT_TRUE(bs16[8]);
	EMB_ASSERT_EQUAL(bs16.count(), 13);
	EMB_ASSERT_TRUE(bs16[10]);
	bs16.flip(10);
	EMB_ASSERT_TRUE(!bs16[10]);
	bs16[10] = true;
	EMB_ASSERT_TRUE(bs16[10]);
	bs16[10] = false;
	EMB_ASSERT_TRUE(!bs16[10]);

	bs32.set(30);
	EMB_ASSERT_TRUE(bs32[30]);
	bs32.flip(30);
	EMB_ASSERT_TRUE(!bs32[30]);
	bs32.set(30);
	bs32.flip();
	EMB_ASSERT_EQUAL(bs32.count(), 31);

	// extra bits invariant
	bs.set();
	EMB_ASSERT_EQUAL(bs.count(), 1);
	EMB_ASSERT_TRUE(bs.all());
	bs.flip();
	EMB_ASSERT_TRUE(bs.none());

	emb::Bitset<4> bs4(0xFFFA);
	EMB_ASSERT_EQUAL(bs4.count(), 2);
	EMB_ASSERT_TRUE(!bs4.all());
	bs4.reset(3);
	EMB_ASSERT_EQUAL(bs4.count(), 1);
	bs4.set();
	EMB_ASSERT_EQUAL(bs4.count(), 4);
	EMB_ASSERT_TRUE(bs4.all());
	bs4.flip();
	EMB_ASSERT_TRUE(bs4.none());
	bs4.flip(1);
	EMB_ASSERT_EQUAL(bs4.count(), 1);
	EMB_ASSERT_TRUE(bs4[1]);
	bs4[1] = false;
	EMB_ASSERT_TRUE(!bs4[1]);

	bs17.set();
	EMB_ASSERT_EQUAL(bs17.count(), 17);

	emb::Bitset<60>bs60(0xFFFFFFFFFFFFFFFF);
	EMB_ASSERT_EQUAL(bs60.count(), 60);
	EMB_ASSERT_TRUE(bs60.all());
	bs60.reset(30);
	EMB_ASSERT_EQUAL(bs60.count(), 59);
	EMB_ASSERT_TRUE(!bs60.all());
	bs60.reset(58);
	EMB_ASSERT_EQUAL(bs60.count(), 58);
	bs60.set();
	EMB_ASSERT_EQUAL(bs60.count(), 60);
	EMB_ASSERT_TRUE(bs60.all());
	bs60.flip(50);
	EMB_ASSERT_TRUE(!bs60[50]);
	bs60.flip(50);
	EMB_ASSERT_TRUE(bs60[50]);
	bs60.flip();
	EMB_ASSERT_TRUE(bs60.none());
	bs60.flip(50);
	EMB_ASSERT_TRUE(bs60[50]);
	bs60.flip();
	EMB_ASSERT_TRUE(!bs60[50]);
	EMB_ASSERT_EQUAL(bs60.count(), 59);

	EMB_ASSERT_TRUE(bs17[5]);
	bs60[50] = bs17[5];
	EMB_ASSERT_TRUE(bs60[50]);

	bs60[50] = bs17[5] = false;
	EMB_ASSERT_TRUE(!bs60[50]);
	EMB_ASSERT_TRUE(!bs17[5]);

	bs60[50] = bs17[5] = true;
	EMB_ASSERT_TRUE(bs60[50]);
	EMB_ASSERT_TRUE(bs17[5]);

	// operator==, !=, =
	EMB_ASSERT_TRUE(emb::Bitset<16>(0xFFCF) == emb::Bitset<16>(0xFFCF));
	EMB_ASSERT_TRUE(emb::Bitset<12>(0xFFCF) == emb::Bitset<12>(0x0FCF));
	EMB_ASSERT_TRUE(emb::Bitset<28>(0xA1ABCDEF) == emb::Bitset<28>(0xB1ABCDEF));

	EMB_ASSERT_TRUE(bs60 != emb::Bitset<60>(0x1));

	emb::Bitset<60> bs60_2;
	emb::Bitset<60> bs60_3 = bs60;
	bs60_2 = bs60_3;
	EMB_ASSERT_TRUE(bs60 == bs60_2);
	bs60_2.flip(35);
	EMB_ASSERT_TRUE(bs60 != bs60_2);
}

