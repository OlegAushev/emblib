#include "emb_tests.h"


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
			if ((i % 2) == 0)
			{
				_data[page][addr/2 + i/2] = _data[page][addr] | buf[i];
			}
			else
			{
				_data[page][addr] = _data[page][addr] | (buf[i] << 8);
			}
		}
	}

	virtual emb::eeprom::Error write(uint16_t page, uint16_t addr, const uint8_t* buf, size_t len, emb::chrono::milliseconds timeout)
	{

	}
};

