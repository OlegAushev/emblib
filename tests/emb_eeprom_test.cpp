#include "emb_tests.h"
#include <c28x_mcu/f2837xd/crc/mcu_crc.h>


class TestingEepromController : public emb::eeprom::IController
{
public:
	static const size_t page_bytes = 64;
	static const size_t page_count = 8;
private:
	static uint16_t _data[page_count][page_bytes/2];
public:
	virtual emb::eeprom::Error read(uint16_t page, uint16_t addr, uint8_t* buf, size_t len, emb::chrono::milliseconds timeout)
	{
		for (size_t i = 0; i < len; ++i)
		{
			if (((addr + i) % 2) == 0)
			{
				buf[i] = _data[page][(addr + i)/2] & 0x00FF;
			}
			else
			{
				buf[i] = _data[page][(addr + i)/2] >> 8;
			}
		}
		return emb::eeprom::Error::no_error;
	}

	virtual emb::eeprom::Error write(uint16_t page, uint16_t addr, const uint8_t* buf, size_t len, emb::chrono::milliseconds timeout)
	{
		for (size_t i = 0; i < len; ++i)
		{
			if (((addr + i) % 2) == 0)
			{
				_data[page][(addr + i)/2] = (_data[page][(addr + i)/2] & 0xFF00) | buf[i];
			}
			else
			{
				_data[page][(addr + i)/2] = (_data[page][(addr + i)/2] & 0x00FF) | (buf[i] << 8);
			}
		}
		return emb::eeprom::Error::no_error;
	}
};


uint16_t TestingEepromController::_data[TestingEepromController::page_count][TestingEepromController::page_bytes/2] = {0};


struct TestingEepromStruct1
{
	uint32_t u32;
	float f32;
	uint16_t u16;
	int32_t i32;
	bool bl;
};


struct TestingEepromStruct2
{
	float f32_1;
	float f32_2;
	float f32_3;
	float f32_4;
	float f32_5;
};


void emb::tests::eeprom()
{
	TestingEepromController eeprom_controller;
	emb::eeprom::Storage eeprom(&eeprom_controller, TestingEepromController::page_bytes, TestingEepromController::page_count, mcu::crc::calc_crc32_byte8);

	TestingEepromStruct1 s1_src = {42, emb::numbers::pi, 12, -100, true};
	TestingEepromStruct2 s2_src = {1.f, -2.f, 3.f, -4.f, 5.f};

	eeprom.write<TestingEepromStruct1>(0, s1_src, emb::chrono::milliseconds(-1));
	eeprom.write<TestingEepromStruct2>(2, s2_src, emb::chrono::milliseconds(-1));

	TestingEepromStruct1 s1_dest = {};
	TestingEepromStruct2 s2_dest = {};
	emb::eeprom::Error read_error1 = eeprom.read<TestingEepromStruct1>(0, s1_dest, emb::chrono::milliseconds(-1));
	emb::eeprom::Error read_error2 = eeprom.read<TestingEepromStruct2>(2, s2_dest, emb::chrono::milliseconds(-1));

	EMB_ASSERT_TRUE(emb::c28x::are_equal(s1_src, s1_dest));
	EMB_ASSERT_TRUE(emb::c28x::are_equal(s2_src, s2_dest));
}





















