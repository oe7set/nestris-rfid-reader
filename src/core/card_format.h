// Player card layout in sector 1 (docs/CARD_FORMAT.md).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace core {

constexpr uint8_t kNameBlock = 4;
constexpr uint8_t kMarkerBlock = 5;
constexpr uint8_t kReservedBlock = 6;
constexpr size_t kBlockSize = 16;
constexpr size_t kMaxName = 15;
constexpr uint8_t kFormatVersion = 1;

enum class CardFormat : uint8_t { Retroverse, Legacy, Blank, Corrupt, Unreadable, Unsupported };

const char* formatName(CardFormat format);

// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no xor-out).
uint16_t crc16(const uint8_t* data, size_t len);

// 1-15 printable ASCII characters without leading/trailing spaces.
bool validName(const std::string& name);

// MIFARE Classic 1K/4K (and SmartMX emulating them), judged by the SAK byte.
bool isMifareClassic(uint8_t sak);

// Blocks 4 and 5 for `name`. Returns false for an invalid name.
bool encodeCard(const std::string& name, uint8_t block4[kBlockSize], uint8_t block5[kBlockSize]);

struct DecodedCard {
    CardFormat format = CardFormat::Blank;
    std::string name;  // empty unless Retroverse or Legacy
};

DecodedCard decodeCard(const uint8_t block4[kBlockSize], const uint8_t block5[kBlockSize]);

}  // namespace core
