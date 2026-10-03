#include "hw/oled.h"

#include <Wire.h>

namespace hw {

using core::DisplayKind;
using core::Screen;

namespace {

constexpr int16_t kWidth = 128;
constexpr uint32_t kFrameMs = 400;            // idle animation step
constexpr uint32_t kShiftMs = 60UL * 1000;    // burn-in: move the idle screen every minute
constexpr uint32_t kDimAfterMs = 10UL * 60 * 1000;
constexpr uint8_t kDimContrast = 8;
const int8_t kShifts[][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}};

// UTF-8 -> the SSD1306 library's CP437 font (only the characters we use).
std::string toCp437(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(in[i]);
        if (c < 0x80) {
            out += static_cast<char>(c);
            continue;
        }
        if (c == 0xC3 && i + 1 < in.size()) {
            const unsigned char d = static_cast<unsigned char>(in[++i]);
            switch (d) {
                case 0xA4: out += '\x84'; break;  // ä
                case 0xB6: out += '\x94'; break;  // ö
                case 0xBC: out += '\x81'; break;  // ü
                case 0x84: out += '\x8E'; break;  // Ä
                case 0x96: out += '\x99'; break;  // Ö
                case 0x9C: out += '\x9A'; break;  // Ü
                case 0x9F: out += '\xE1'; break;  // ß
                default: out += '?';
            }
            continue;
        }
        out += '?';
        while (i + 1 < in.size() && (static_cast<unsigned char>(in[i + 1]) & 0xC0) == 0x80) ++i;
    }
    return out;
}

}  // namespace

uint8_t Oled::probeAddress() {
    for (uint8_t addr : {0x3C, 0x3D}) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) return addr;
    }
    return 0;
}

DisplayKind Oled::configure(DisplayKind wanted, bool flip, uint8_t brightness) {
    if (!wireStarted_) {
        Wire.begin(sda_, scl_);
        Wire.setClock(400000);
        wireStarted_ = true;
    }
    delete oled_;
    oled_ = nullptr;
    kind_ = DisplayKind::None;
    brightness_ = brightness;
    if (wanted == DisplayKind::None) return kind_;

    const uint8_t addr = probeAddress();
    if (addr == 0) return kind_;  // no OLED attached: the reader works without it

    const int16_t height = wanted == DisplayKind::Oled128x64 ? 64 : 32;
    oled_ = new Adafruit_SSD1306(kWidth, height, &Wire, -1);
    if (!oled_->begin(SSD1306_SWITCHCAPVCC, addr)) {
        delete oled_;
        oled_ = nullptr;
        return kind_;
    }
    kind_ = wanted;
    oled_->cp437(true);
    oled_->setTextWrap(false);
    oled_->setRotation(flip ? 2 : 0);
    setContrast(brightness_);
    dimmed_ = false;
    if (hasScreen_) draw();
    return kind_;
}

void Oled::setContrast(uint8_t value) {
    if (!oled_) return;
    oled_->ssd1306_command(SSD1306_SETCONTRAST);
    oled_->ssd1306_command(value);
}

void Oled::show(const Screen& screen, uint32_t nowMs) {
    const bool wasIdle = hasScreen_ && screen_.idle;
    screen_ = screen;
    hasScreen_ = true;
    if (!screen.idle || !wasIdle) {
        idleSinceMs_ = nowMs;
        lastShiftMs_ = nowMs;
    }
    if (dimmed_ && !screen.idle) {
        setContrast(brightness_);
        dimmed_ = false;
    }
    draw();
}

void Oled::tick(uint32_t nowMs) {
    if (!oled_ || !hasScreen_ || !screen_.idle) return;
    bool redraw = false;
    if (screen_.icon == core::Icon::CardAnimation && kind_ == DisplayKind::Oled128x64 &&
        nowMs - lastFrameMs_ >= kFrameMs) {
        lastFrameMs_ = nowMs;
        frame_ = (frame_ + 1) % 8;
        redraw = true;
    }
    if (nowMs - lastShiftMs_ >= kShiftMs) {
        lastShiftMs_ = nowMs;
        shift_ = (shift_ + 1) % (sizeof(kShifts) / sizeof(kShifts[0]));
        redraw = true;
    }
    if (!dimmed_ && nowMs - idleSinceMs_ >= kDimAfterMs) {
        setContrast(kDimContrast);
        dimmed_ = true;
    }
    if (redraw) draw();
}

void Oled::drawLine(const std::string& utf8, uint8_t size, int16_t y) {
    std::string text = toCp437(utf8);
    const size_t maxChars = static_cast<size_t>(kWidth / (6 * size));
    if (text.size() > maxChars) text.resize(maxChars);
    const int16_t dx = screen_.idle ? kShifts[shift_][0] : 0;
    const int16_t dy = screen_.idle ? kShifts[shift_][1] : 0;
    const int16_t width = static_cast<int16_t>(text.size() * 6 * size);
    oled_->setTextSize(size);
    oled_->setCursor((kWidth - width) / 2 + dx, y + dy);
    oled_->print(text.c_str());
}

void Oled::draw() {
    if (!oled_) return;
    oled_->clearDisplay();
    oled_->setTextColor(SSD1306_WHITE);

    const Screen& s = screen_;
    // Large text (12x16 px per character) only if the line fits (10 characters).
    const bool big = s.firstBig && s.count > 0 && toCp437(s.lines[0]).size() <= 10;

    if (kind_ == DisplayKind::Oled128x32) {
        if (big) {
            drawLine(s.lines[0], 2, 0);
            if (s.count > 1) drawLine(s.lines[1], 1, 22);
        } else {
            if (s.count > 0) drawLine(s.lines[0], 1, s.count > 1 ? 6 : 12);
            if (s.count > 1) drawLine(s.lines[1], 1, 19);
        }
    } else {
        // 128x64: status bar, then up to four lines.
        if (!s.status.empty()) {
            oled_->setTextSize(1);
            oled_->setCursor(0, 0);
            oled_->print(toCp437(s.status).c_str());
            oled_->drawFastHLine(0, 10, kWidth, SSD1306_WHITE);
        }
        int16_t y = 14;
        for (size_t i = 0; i < s.count; ++i) {
            const uint8_t size = (i == 0 && big) ? 2 : 1;
            drawLine(s.lines[i], size, y);
            y += size == 2 ? 20 : 12;
        }
        if (s.icon == core::Icon::CardAnimation && y <= 50) {
            // A small card that slowly "drops" onto the reader line.
            const int16_t bob = static_cast<int16_t>(frame_ < 4 ? frame_ : 8 - frame_);
            oled_->drawRoundRect(54, 50 + bob - 4, 20, 11, 2, SSD1306_WHITE);
            oled_->drawFastHLine(44, 63, 40, SSD1306_WHITE);
        }
    }
    oled_->display();
}

}  // namespace hw
