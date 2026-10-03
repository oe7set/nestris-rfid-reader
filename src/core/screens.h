// What the OLED shows in each state (docs/ARCHITECTURE.md#display).
// Only texts and layout hints; hw/oled draws them.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "core/card_format.h"
#include "core/settings.h"

namespace core {

enum class Phase : uint8_t { Boot, Idle, Card, Writing, WriteOk, WriteFailed, ReaderMissing };

struct View {
    Phase phase = Phase::Boot;
    Lang lang = Lang::De;
    uint8_t rows = 2;  // text lines the display has: 2 (128x32) or 4 (128x64)
    bool hostConnected = false;
    std::string fwVersion;
    // Card / WriteOk
    CardFormat format = CardFormat::Blank;
    std::string name;
    std::string hostLines[4];  // from `show`; empty = none
    size_t hostLineCount = 0;
    // WriteFailed: protocol error code (timeout, verify, ...)
    std::string error;
};

enum class Icon : uint8_t { None, CardAnimation, Ok, Error };

struct Screen {
    static constexpr size_t kMaxLines = 4;
    std::string lines[kMaxLines];
    size_t count = 0;
    bool firstBig = false;  // draw the first line large if it fits
    Icon icon = Icon::None;
    std::string status;     // status bar (128x64 only), e.g. "PC getrennt"
    bool idle = false;      // burn-in protection applies

    bool operator==(const Screen& o) const;
    bool operator!=(const Screen& o) const { return !(*this == o); }
};

Screen buildScreen(const View& view);

}  // namespace core
