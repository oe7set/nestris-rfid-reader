// Card UID: 4, 7 or 10 bytes, printed as uppercase hex without separators.
#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace core {

struct Uid {
    static constexpr uint8_t kMaxSize = 10;
    uint8_t bytes[kMaxSize] = {};
    uint8_t size = 0;

    Uid() = default;
    Uid(const uint8_t* data, uint8_t n) {
        size = n > kMaxSize ? kMaxSize : n;
        std::memcpy(bytes, data, size);
    }

    bool empty() const { return size == 0; }

    bool operator==(const Uid& other) const {
        return size == other.size && std::memcmp(bytes, other.bytes, size) == 0;
    }
    bool operator!=(const Uid& other) const { return !(*this == other); }

    std::string hex() const {
        static const char digits[] = "0123456789ABCDEF";
        std::string out;
        out.reserve(size * 2);
        for (uint8_t i = 0; i < size; ++i) {
            out += digits[bytes[i] >> 4];
            out += digits[bytes[i] & 0x0F];
        }
        return out;
    }

    // Parses 8, 14 or 20 hex digits (any case). Returns false otherwise.
    static bool parse(const std::string& text, Uid& out) {
        const size_t n = text.size();
        if (n != 8 && n != 14 && n != 20) return false;
        uint8_t buf[kMaxSize];
        for (size_t i = 0; i < n; i += 2) {
            int hi = nibble(text[i]);
            int lo = nibble(text[i + 1]);
            if (hi < 0 || lo < 0) return false;
            buf[i / 2] = static_cast<uint8_t>((hi << 4) | lo);
        }
        out = Uid(buf, static_cast<uint8_t>(n / 2));
        return true;
    }

private:
    static int nibble(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
};

}  // namespace core
