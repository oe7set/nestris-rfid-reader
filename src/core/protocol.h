// Protocol v2: parsing host commands and building reader messages
// (docs/PROTOCOL.md). Pure C++ + ArduinoJson, unit-tested on the PC.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "core/card_format.h"
#include "core/settings.h"
#include "core/uid.h"

namespace core {

constexpr int kProtocolVersion = 2;
constexpr size_t kMaxLine = 512;
constexpr size_t kMaxShowLines = 4;
constexpr size_t kMaxShowChars = 21;
constexpr uint32_t kDefaultWriteTimeoutMs = 15000;

enum class CmdType : uint8_t { Hello, Ping, Write, Show, Cancel, Config, Reboot, Unknown, Invalid };

struct ConfigChange {
    std::optional<DisplayKind> display;
    std::optional<Lang> lang;
    std::optional<uint16_t> removedAfterMs;
    std::optional<uint8_t> brightness;
    std::optional<bool> flip;
    std::optional<bool> debug;
};

struct Command {
    CmdType type = CmdType::Invalid;
    std::optional<int32_t> id;
    // Invalid: error code (bad_json / bad_request) + detail.
    std::string error;
    std::string detail;
    // write
    std::string name;
    std::optional<Uid> uid;  // write, show
    uint32_t timeoutMs = kDefaultWriteTimeoutMs;
    // show
    std::string lines[kMaxShowLines];
    size_t lineCount = 0;
    uint32_t ttlMs = 0;
    // config
    ConfigChange config;
};

Command parseCommand(const std::string& line);

// The card as reported in `card` / `hello` / `status`.
struct CardInfo {
    Uid uid;
    CardFormat format = CardFormat::Blank;
    std::string name;  // empty = null
};

struct DeviceInfo {
    std::string fw;
    std::string build;
    std::string board;
    std::string serial;
};

struct ReaderState {
    bool readerOk = true;
    uint8_t chipVersion = 0;
    DisplayKind display = DisplayKind::None;  // what is actually attached
    Lang lang = Lang::De;
};

std::string msgHello(const DeviceInfo& dev, const ReaderState& st, const CardInfo* card,
                     std::optional<int32_t> id);
std::string msgCardPresent(const CardInfo& card);
std::string msgCardRemoved(const Uid& uid);
std::string msgStatus(const CardInfo* card, bool readerOk, uint32_t uptimeS, bool host);
std::string msgResultOk(std::optional<int32_t> id);
std::string msgResultOkCard(std::optional<int32_t> id, const Uid& uid, const std::string& name);
std::string msgResultError(std::optional<int32_t> id, const char* error, const std::string& detail = "");
std::string msgLog(const char* level, const std::string& text);

}  // namespace core
