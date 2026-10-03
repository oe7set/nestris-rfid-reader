// Persistent settings (NVS), changed by the `config` command (docs/PROTOCOL.md).
#pragma once

#include <cstdint>
#include <string>

namespace core {

enum class DisplayKind : uint8_t { Oled128x32, Oled128x64, None };
enum class Lang : uint8_t { De, En };

const char* displayName(DisplayKind kind);
bool parseDisplay(const std::string& text, DisplayKind& out);
const char* langName(Lang lang);
bool parseLang(const std::string& text, Lang& out);

struct Settings {
    DisplayKind display = DisplayKind::Oled128x32;
    Lang lang = Lang::De;
    uint16_t removedAfterMs = 300;
    uint8_t brightness = 200;
    bool flip = false;
    bool debug = false;

    static constexpr uint16_t kMinRemovedAfterMs = 150;
    static constexpr uint16_t kMaxRemovedAfterMs = 2000;
};

}  // namespace core
