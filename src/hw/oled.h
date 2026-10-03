// SSD1306 OLED (128x32 or 128x64, I2C): draws core::Screen models.
#pragma once

#include <Adafruit_SSD1306.h>

#include "core/app.h"

namespace hw {

class Oled : public core::IDisplay {
public:
    Oled(uint8_t sdaPin, uint8_t sclPin) : sda_(sdaPin), scl_(sclPin) {}

    core::DisplayKind configure(core::DisplayKind wanted, bool flip, uint8_t brightness) override;
    void show(const core::Screen& screen, uint32_t nowMs) override;
    void tick(uint32_t nowMs) override;

private:
    void draw();
    void drawLine(const std::string& utf8, uint8_t size, int16_t y);
    void setContrast(uint8_t value);
    uint8_t probeAddress();

    uint8_t sda_;
    uint8_t scl_;
    bool wireStarted_ = false;
    Adafruit_SSD1306* oled_ = nullptr;
    core::DisplayKind kind_ = core::DisplayKind::None;
    uint8_t brightness_ = 200;

    core::Screen screen_;
    bool hasScreen_ = false;
    uint32_t idleSinceMs_ = 0;
    uint32_t lastFrameMs_ = 0;
    uint8_t frame_ = 0;       // idle animation
    uint8_t shift_ = 0;       // burn-in offset index
    uint32_t lastShiftMs_ = 0;
    bool dimmed_ = false;
};

}  // namespace hw
