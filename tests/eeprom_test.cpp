#if defined(EMBLIB_C28X)
#include <mcudrv/c28x/f2837xd/crc/crc.h>
#elif defined(STM32H7xx)
#include <mcudrv/stm32/h7/crc/crc.h>
#elif defined(STM32F4xx)
#include <mcudrv/stm32/f4/crc/crc.h>
#elif defined(APM32F4xx)
#include <mcudrv/apm32/f4/crc/crc.h>
#endif


#include <emblib/tests/tests.hpp>


class TestingEepromDriver : public emb::mem::eeprom::driver {
private:
    static const size_t _page_bytes = 64;
    static const size_t _page_count = 8;
    static uint16_t _data[_page_count][_page_bytes/2];
public:
    virtual emb::mem::status read(size_t page, size_t addr, uint8_t* buf, size_t len, EMB_MILLISECONDS timeout) EMB_OVERRIDE  {
        for (size_t i = 0; i < len; ++i) {
            if (((addr + i) % 2) == 0) {
                buf[i] = uint8_t(_data[page][(addr + i)/2] & 0x00FF);
            } else {
                buf[i] = uint8_t(_data[page][(addr + i)/2] >> 8);
            }
        }
        return emb::mem::status::ok;
    }

    virtual emb::mem::status write(size_t page, size_t addr, const uint8_t* buf, size_t len, EMB_MILLISECONDS timeout) EMB_OVERRIDE {
        for (size_t i = 0; i < len; ++i) {
            if (((addr + i) % 2) == 0) {
                _data[page][(addr + i)/2] = (_data[page][(addr + i)/2] & 0xFF00) | buf[i];
            } else {
                _data[page][(addr + i)/2] = (_data[page][(addr + i)/2] & 0x00FF) | (buf[i] << 8);
            }
        }
        return emb::mem::status::ok;
    }

    virtual size_t page_bytes() const EMB_OVERRIDE { return _page_bytes; }
    virtual size_t page_count() const EMB_OVERRIDE { return _page_count; }
};


uint16_t TestingEepromDriver::_data[TestingEepromDriver::_page_count][TestingEepromDriver::_page_bytes/2] = {0};


struct TestingEepromStruct1
{
    uint32_t u32;
    float f32;
    uint16_t u16;
    int32_t i32;
    bool bl;
#ifndef EMBLIB_C28X
    friend auto operator<=>(const TestingEepromStruct1&, const TestingEepromStruct1&) = default;
#endif
};


struct TestingEepromStruct2
{
    float f32_1;
    float f32_2;
    float f32_3;
    float f32_4;
    float f32_5;
#ifndef EMBLIB_C28X
    friend auto operator<=>(const TestingEepromStruct2&, const TestingEepromStruct2&) = default;
#endif
};


void emb::tests::eeprom_test() {
#ifdef EMB_TESTS_ENABLED
    TestingEepromDriver eeprom_driver;
#if defined(EMBLIB_C28X)
    emb::mem::eeprom::storage eeprom(eeprom_driver, mcu::c28x::crc::calc_crc32_byte8);
#elif defined(EMBLIB_ARM)
    emb::mem::eeprom::storage eeprom(eeprom_driver, mcu::crc::calc_crc32);
#endif

    TestingEepromStruct1 s1_src = {42, emb::numbers::pi, 12, -100, true};
    TestingEepromStruct2 s2_src = {1.f, -2.f, 3.f, -4.f, 5.f};

    eeprom.write<TestingEepromStruct1>(0, s1_src, EMB_MILLISECONDS(-1));
    eeprom.write<TestingEepromStruct2>(2, s2_src, EMB_MILLISECONDS(-1));

    TestingEepromStruct1 s1_dest = {};
    TestingEepromStruct2 s2_dest = {};
    EMB_MAYBE_UNUSED emb::mem::status read_sts1 = eeprom.read<TestingEepromStruct1>(0, s1_dest, EMB_MILLISECONDS(-1));
    EMB_MAYBE_UNUSED emb::mem::status read_sts2 = eeprom.read<TestingEepromStruct2>(2, s2_dest, EMB_MILLISECONDS(-1));

#if defined(EMBLIB_C28X)
    EMB_ASSERT_TRUE(emb::c28x::are_equal(s1_src, s1_dest));
    EMB_ASSERT_TRUE(emb::c28x::are_equal(s2_src, s2_dest));
#else
    EMB_ASSERT_EQUAL(s1_src, s1_dest);
    EMB_ASSERT_EQUAL(s2_src, s2_dest);
#endif

#endif
}
