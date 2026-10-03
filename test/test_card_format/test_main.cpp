#include <unity.h>

#include <cstring>

#include "core/card_format.h"

using namespace core;

void setUp() {}
void tearDown() {}

static void test_crc16_ccitt_false_check_value() {
    const char* check = "123456789";
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16(reinterpret_cast<const uint8_t*>(check), 9));
}

static void test_encode_layout() {
    uint8_t b4[16], b5[16];
    TEST_ASSERT_TRUE(encodeCard("Erv", b4, b5));
    const uint8_t name[16] = {'E', 'r', 'v'};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(name, b4, 16);
    TEST_ASSERT_EQUAL_MEMORY("RVRS", b5, 4);
    TEST_ASSERT_EQUAL_UINT8(1, b5[4]);
    TEST_ASSERT_EQUAL_UINT8(3, b5[5]);
    const uint16_t crc = crc16(b4, 16);
    TEST_ASSERT_EQUAL_UINT8(crc >> 8, b5[6]);
    TEST_ASSERT_EQUAL_UINT8(crc & 0xFF, b5[7]);
    for (int i = 8; i < 16; ++i) TEST_ASSERT_EQUAL_UINT8(0, b5[i]);
}

static void test_roundtrip_retroverse() {
    uint8_t b4[16], b5[16];
    encodeCard("FramesBond 2026", b4, b5);
    DecodedCard d = decodeCard(b4, b5);
    TEST_ASSERT_EQUAL_STRING("retroverse", formatName(d.format));
    TEST_ASSERT_EQUAL_STRING("FramesBond 2026", d.name.c_str());
}

static void test_invalid_names_are_refused() {
    uint8_t b4[16], b5[16];
    TEST_ASSERT_FALSE(encodeCard("", b4, b5));
    TEST_ASSERT_FALSE(encodeCard("sixteen chars!!!", b4, b5));
    TEST_ASSERT_FALSE(encodeCard(" lead", b4, b5));
    TEST_ASSERT_FALSE(encodeCard("trail ", b4, b5));
    TEST_ASSERT_FALSE(encodeCard("J\xC3\xBCrgen", b4, b5));  // non-ASCII
}

static void test_crc_mismatch_is_corrupt() {
    uint8_t b4[16], b5[16];
    encodeCard("Erv", b4, b5);
    b4[0] = 'X';  // name changed without updating the marker
    TEST_ASSERT_EQUAL(CardFormat::Corrupt, decodeCard(b4, b5).format);
}

static void test_legacy_v1_card() {
    uint8_t b4[16] = {'E', 'r', 'v'};
    uint8_t b5[16] = {};
    DecodedCard d = decodeCard(b4, b5);
    TEST_ASSERT_EQUAL(CardFormat::Legacy, d.format);
    TEST_ASSERT_EQUAL_STRING("Erv", d.name.c_str());
    uint8_t full[16];
    std::memcpy(full, "SixteenCharsName", 16);  // v1 allowed 16 characters
    d = decodeCard(full, b5);
    TEST_ASSERT_EQUAL(CardFormat::Legacy, d.format);
    TEST_ASSERT_EQUAL_STRING("SixteenCharsName", d.name.c_str());
}

static void test_blank_cards() {
    uint8_t zeros[16] = {};
    uint8_t ff[16];
    std::memset(ff, 0xFF, 16);
    uint8_t spaces[16] = {' ', ' ', ' '};
    TEST_ASSERT_EQUAL(CardFormat::Blank, decodeCard(zeros, zeros).format);
    TEST_ASSERT_EQUAL(CardFormat::Blank, decodeCard(ff, ff).format);
    TEST_ASSERT_EQUAL(CardFormat::Blank, decodeCard(spaces, zeros).format);
    uint8_t garbage[16] = {'A', 'B', 0, 'C'};  // data after the terminator
    TEST_ASSERT_EQUAL(CardFormat::Blank, decodeCard(garbage, zeros).format);
}

static void test_mifare_classic_sak() {
    TEST_ASSERT_TRUE(isMifareClassic(0x08));
    TEST_ASSERT_TRUE(isMifareClassic(0x18));
    TEST_ASSERT_TRUE(isMifareClassic(0x88));
    TEST_ASSERT_FALSE(isMifareClassic(0x00));  // Ultralight
    TEST_ASSERT_FALSE(isMifareClassic(0x20));  // ISO 14443-4 (phones, DESFire)
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_crc16_ccitt_false_check_value);
    RUN_TEST(test_encode_layout);
    RUN_TEST(test_roundtrip_retroverse);
    RUN_TEST(test_invalid_names_are_refused);
    RUN_TEST(test_crc_mismatch_is_corrupt);
    RUN_TEST(test_legacy_v1_card);
    RUN_TEST(test_blank_cards);
    RUN_TEST(test_mifare_classic_sak);
    return UNITY_END();
}
