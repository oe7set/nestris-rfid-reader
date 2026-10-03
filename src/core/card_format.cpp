#include "core/card_format.h"

#include <cstring>

namespace core {

namespace {

const uint8_t kMagic[4] = {'R', 'V', 'R', 'S'};

bool printable(uint8_t c) { return c >= 0x20 && c <= 0x7E; }

}  // namespace

const char* formatName(CardFormat format) {
    switch (format) {
        case CardFormat::Retroverse: return "retroverse";
        case CardFormat::Legacy: return "legacy";
        case CardFormat::Blank: return "blank";
        case CardFormat::Corrupt: return "corrupt";
        case CardFormat::Unreadable: return "unreadable";
        case CardFormat::Unsupported: return "unsupported";
    }
    return "unreadable";
}

uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

bool validName(const std::string& name) {
    if (name.empty() || name.size() > kMaxName) return false;
    if (name.front() == ' ' || name.back() == ' ') return false;
    for (char c : name) {
        if (!printable(static_cast<uint8_t>(c))) return false;
    }
    return true;
}

bool isMifareClassic(uint8_t sak) {
    switch (sak & 0x7F) {  // bit 7 is set by some Infineon chips
        case 0x08:  // Classic 1K
        case 0x18:  // Classic 4K
        case 0x28:  // SmartMX with Classic 1K emulation
        case 0x38:  // SmartMX with Classic 4K emulation
            return true;
        default:
            return false;
    }
}

bool encodeCard(const std::string& name, uint8_t block4[kBlockSize], uint8_t block5[kBlockSize]) {
    if (!validName(name)) return false;
    std::memset(block4, 0, kBlockSize);
    std::memcpy(block4, name.data(), name.size());
    std::memset(block5, 0, kBlockSize);
    std::memcpy(block5, kMagic, sizeof(kMagic));
    block5[4] = kFormatVersion;
    block5[5] = static_cast<uint8_t>(name.size());
    const uint16_t crc = crc16(block4, kBlockSize);
    block5[6] = static_cast<uint8_t>(crc >> 8);
    block5[7] = static_cast<uint8_t>(crc & 0xFF);
    return true;
}

DecodedCard decodeCard(const uint8_t block4[kBlockSize], const uint8_t block5[kBlockSize]) {
    DecodedCard out;

    if (std::memcmp(block5, kMagic, sizeof(kMagic)) == 0) {
        out.format = CardFormat::Corrupt;
        const uint8_t len = block5[5];
        const uint16_t crc = static_cast<uint16_t>((block5[6] << 8) | block5[7]);
        if (block5[4] != kFormatVersion || len < 1 || len > kMaxName) return out;
        if (crc != crc16(block4, kBlockSize)) return out;
        for (size_t i = len; i < kBlockSize; ++i) {
            if (block4[i] != 0) return out;
        }
        std::string name(reinterpret_cast<const char*>(block4), len);
        if (!validName(name)) return out;
        out.format = CardFormat::Retroverse;
        out.name = name;
        return out;
    }

    // No marker: a v1 card holds a name followed by zeros.
    size_t end = 0;
    while (end < kBlockSize && block4[end] != 0) {
        if (!printable(block4[end])) return out;  // garbage or 0xFF: blank
        ++end;
    }
    for (size_t i = end; i < kBlockSize; ++i) {
        if (block4[i] != 0) return out;
    }
    std::string name(reinterpret_cast<const char*>(block4), end);
    const size_t first = name.find_first_not_of(' ');
    if (first == std::string::npos) return out;  // empty or only spaces
    const size_t last = name.find_last_not_of(' ');
    out.format = CardFormat::Legacy;
    out.name = name.substr(first, last - first + 1);
    return out;
}

}  // namespace core
