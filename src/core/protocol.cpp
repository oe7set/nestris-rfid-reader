#include "core/protocol.h"

#include <ArduinoJson.h>

#include <cstring>

namespace core {

namespace {

Command invalid(std::optional<int32_t> id, const char* error, const std::string& detail) {
    Command c;
    c.type = CmdType::Invalid;
    c.id = id;
    c.error = error;
    c.detail = detail;
    return c;
}

// Integer field in [lo, hi]; absent -> nullopt with ok = true.
std::optional<long> intField(JsonVariantConst v, long lo, long hi, bool& ok) {
    if (v.isNull()) return std::nullopt;
    if (!v.is<long>()) {
        ok = false;
        return std::nullopt;
    }
    const long n = v.as<long>();
    if (n < lo || n > hi) {
        ok = false;
        return std::nullopt;
    }
    return n;
}

std::string clip(const char* text) {
    std::string out;
    for (const char* p = text; *p && out.size() < kMaxShowChars; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x20) continue;
        out += c < 0x7F ? static_cast<char>(c) : '?';  // the OLED font is ASCII
        if (c >= 0x80) {
            while ((static_cast<unsigned char>(p[1]) & 0xC0) == 0x80) ++p;  // skip UTF-8 continuation bytes
        }
    }
    return out;
}

void putCard(JsonObject obj, const CardInfo& card) {
    obj["uid"] = card.uid.hex();
    if (card.name.empty()) {
        obj["name"] = nullptr;
    } else {
        obj["name"] = card.name;
    }
    obj["format"] = formatName(card.format);
}

void putId(JsonDocument& doc, std::optional<int32_t> id) {
    if (id) {
        doc["id"] = *id;
    } else {
        doc["id"] = nullptr;
    }
}

std::string dump(const JsonDocument& doc) {
    std::string out;
    serializeJson(doc, out);
    return out;
}

std::string hex2(uint8_t v) {
    static const char digits[] = "0123456789ABCDEF";
    std::string s = "0x";
    s += digits[v >> 4];
    s += digits[v & 0x0F];
    return s;
}

}  // namespace

const char* displayName(DisplayKind kind) {
    switch (kind) {
        case DisplayKind::Oled128x32: return "128x32";
        case DisplayKind::Oled128x64: return "128x64";
        case DisplayKind::None: return "none";
    }
    return "none";
}

bool parseDisplay(const std::string& text, DisplayKind& out) {
    if (text == "128x32") out = DisplayKind::Oled128x32;
    else if (text == "128x64") out = DisplayKind::Oled128x64;
    else if (text == "none") out = DisplayKind::None;
    else return false;
    return true;
}

const char* langName(Lang lang) { return lang == Lang::En ? "en" : "de"; }

bool parseLang(const std::string& text, Lang& out) {
    if (text == "de") out = Lang::De;
    else if (text == "en") out = Lang::En;
    else return false;
    return true;
}

Command parseCommand(const std::string& line) {
    if (line.size() > kMaxLine) return invalid(std::nullopt, "line_too_long", "");

    JsonDocument doc;
    if (deserializeJson(doc, line) != DeserializationError::Ok || !doc.is<JsonObject>()) {
        return invalid(std::nullopt, "bad_json", "");
    }

    std::optional<int32_t> id;
    bool ok = true;
    if (auto n = intField(doc["id"], 0, 2147483647L, ok)) id = static_cast<int32_t>(*n);
    if (!ok) return invalid(std::nullopt, "bad_request", "id");

    const char* type = doc["type"].is<const char*>() ? doc["type"].as<const char*>() : nullptr;
    if (type == nullptr) return invalid(id, "bad_request", "type");

    Command c;
    c.id = id;

    if (std::strcmp(type, "hello") == 0) {
        c.type = CmdType::Hello;
    } else if (std::strcmp(type, "ping") == 0) {
        c.type = CmdType::Ping;
    } else if (std::strcmp(type, "cancel") == 0) {
        c.type = CmdType::Cancel;
    } else if (std::strcmp(type, "reboot") == 0) {
        c.type = CmdType::Reboot;
    } else if (std::strcmp(type, "write") == 0) {
        c.type = CmdType::Write;
        if (!doc["name"].is<const char*>()) return invalid(id, "bad_request", "name");
        c.name = doc["name"].as<const char*>();
        if (!validName(c.name)) return invalid(id, "bad_request", "name");
        if (!doc["uid"].isNull()) {
            Uid uid;
            if (!doc["uid"].is<const char*>() || !Uid::parse(doc["uid"].as<const char*>(), uid)) {
                return invalid(id, "bad_request", "uid");
            }
            c.uid = uid;
        }
        if (auto n = intField(doc["timeout_ms"], 1000, 60000, ok)) c.timeoutMs = static_cast<uint32_t>(*n);
        if (!ok) return invalid(id, "bad_request", "timeout_ms");
    } else if (std::strcmp(type, "show") == 0) {
        c.type = CmdType::Show;
        JsonArrayConst lines = doc["lines"].as<JsonArrayConst>();
        if (lines.isNull() || lines.size() < 1 || lines.size() > kMaxShowLines) {
            return invalid(id, "bad_request", "lines");
        }
        for (JsonVariantConst v : lines) {
            if (!v.is<const char*>()) return invalid(id, "bad_request", "lines");
            c.lines[c.lineCount++] = clip(v.as<const char*>());
        }
        if (!doc["uid"].isNull()) {
            Uid uid;
            if (!doc["uid"].is<const char*>() || !Uid::parse(doc["uid"].as<const char*>(), uid)) {
                return invalid(id, "bad_request", "uid");
            }
            c.uid = uid;
        }
        if (auto n = intField(doc["ttl_ms"], 0, 600000, ok)) c.ttlMs = static_cast<uint32_t>(*n);
        if (!ok) return invalid(id, "bad_request", "ttl_ms");
    } else if (std::strcmp(type, "config") == 0) {
        c.type = CmdType::Config;
        ConfigChange& cfg = c.config;
        if (!doc["display"].isNull()) {
            DisplayKind kind;
            if (!doc["display"].is<const char*>() || !parseDisplay(doc["display"].as<const char*>(), kind)) {
                return invalid(id, "bad_request", "display");
            }
            cfg.display = kind;
        }
        if (!doc["lang"].isNull()) {
            Lang lang;
            if (!doc["lang"].is<const char*>() || !parseLang(doc["lang"].as<const char*>(), lang)) {
                return invalid(id, "bad_request", "lang");
            }
            cfg.lang = lang;
        }
        if (auto n = intField(doc["removed_after_ms"], Settings::kMinRemovedAfterMs,
                              Settings::kMaxRemovedAfterMs, ok)) {
            cfg.removedAfterMs = static_cast<uint16_t>(*n);
        }
        if (!ok) return invalid(id, "bad_request", "removed_after_ms");
        if (auto n = intField(doc["brightness"], 0, 255, ok)) cfg.brightness = static_cast<uint8_t>(*n);
        if (!ok) return invalid(id, "bad_request", "brightness");
        for (const char* key : {"flip", "debug"}) {
            JsonVariantConst v = doc[key];
            if (v.isNull()) continue;
            if (!v.is<bool>()) return invalid(id, "bad_request", key);
            (std::strcmp(key, "flip") == 0 ? cfg.flip : cfg.debug) = v.as<bool>();
        }
    } else {
        c.type = CmdType::Unknown;
        c.detail = type;
    }
    return c;
}

std::string msgHello(const DeviceInfo& dev, const ReaderState& st, const CardInfo* card,
                     std::optional<int32_t> id) {
    JsonDocument doc;
    doc["type"] = "hello";
    doc["proto"] = kProtocolVersion;
    doc["fw"] = dev.fw;
    doc["build"] = dev.build;
    doc["board"] = dev.board;
    doc["serial"] = dev.serial;
    doc["display"] = displayName(st.display);
    doc["lang"] = langName(st.lang);
    doc["reader"] = st.readerOk ? "ok" : "missing";
    doc["chip"] = hex2(st.chipVersion);
    if (card) {
        putCard(doc["card"].to<JsonObject>(), *card);
    } else {
        doc["card"] = nullptr;
    }
    if (id) doc["id"] = *id;
    return dump(doc);
}

std::string msgCardPresent(const CardInfo& card) {
    JsonDocument doc;
    doc["type"] = "card";
    doc["state"] = "present";
    putCard(doc.as<JsonObject>(), card);
    return dump(doc);
}

std::string msgCardRemoved(const Uid& uid) {
    JsonDocument doc;
    doc["type"] = "card";
    doc["state"] = "removed";
    doc["uid"] = uid.hex();
    return dump(doc);
}

std::string msgStatus(const CardInfo* card, bool readerOk, uint32_t uptimeS, bool host) {
    JsonDocument doc;
    doc["type"] = "status";
    if (card) {
        putCard(doc["card"].to<JsonObject>(), *card);
    } else {
        doc["card"] = nullptr;
    }
    doc["reader"] = readerOk ? "ok" : "missing";
    doc["uptime_s"] = uptimeS;
    doc["host"] = host;
    return dump(doc);
}

std::string msgResultOk(std::optional<int32_t> id) {
    JsonDocument doc;
    doc["type"] = "result";
    putId(doc, id);
    doc["ok"] = true;
    return dump(doc);
}

std::string msgResultOkCard(std::optional<int32_t> id, const Uid& uid, const std::string& name) {
    JsonDocument doc;
    doc["type"] = "result";
    putId(doc, id);
    doc["ok"] = true;
    doc["uid"] = uid.hex();
    doc["name"] = name;
    return dump(doc);
}

std::string msgResultError(std::optional<int32_t> id, const char* error, const std::string& detail) {
    JsonDocument doc;
    doc["type"] = "result";
    putId(doc, id);
    doc["ok"] = false;
    doc["error"] = error;
    if (!detail.empty()) doc["detail"] = detail;
    return dump(doc);
}

std::string msgLog(const char* level, const std::string& text) {
    JsonDocument doc;
    doc["type"] = "log";
    doc["level"] = level;
    doc["msg"] = text;
    return dump(doc);
}

}  // namespace core
