#include "core/screens.h"

#include <cstring>

namespace core {

namespace {

struct Text {
    const char* key;
    const char* de;
    const char* en;
};

// Display texts. The OLED font is CP437: umlauts are mapped by hw/oled.
const Text kTexts[] = {
    {"place1", "Karte", "Place"},
    {"place2", "auflegen", "your card"},
    {"detected", "Karte erkannt", "Card detected"},
    {"new1", "Neue Karte", "New card"},
    {"new2", "am Terminal anmelden", "sign up at the terminal"},
    {"bad1", "Karte defekt", "Card damaged"},
    {"bad2", "neu beschreiben", "please rewrite"},
    {"unsup1", "Karte nicht", "Card not"},
    {"unsup2", "unterst\xC3\xBCtzt", "supported"},
    {"writing1", "Schreibe ...", "Writing ..."},
    {"writing2", "Karte liegen lassen", "keep the card here"},
    {"saved", "Gespeichert", "Saved"},
    {"failed", "Fehler", "Error"},
    {"rfid1", "Leser-Fehler", "Reader error"},
    {"rfid2", "RFID-Modul pr\xC3\xBC" "fen", "check RFID module"},
    {"nohost", "PC getrennt", "PC disconnected"},
    {"host", "PC verbunden", "PC connected"},
    {"err_timeout", "keine Karte", "no card"},
    {"err_wrong_card", "falsche Karte", "wrong card"},
    {"err_unsupported_card", "Karte nicht unterst\xC3\xBCtzt", "card not supported"},
    {"err_auth", "Karte gesperrt", "card locked"},
    {"err_write", "Karte bewegt?", "card moved?"},
    {"err_verify", "Pr\xC3\xBC" "fung fehlgeschlagen", "verify failed"},
    {"err_cancelled", "abgebrochen", "cancelled"},
};

const char* tr(Lang lang, const char* key) {
    for (const Text& t : kTexts) {
        if (std::strcmp(t.key, key) == 0) return lang == Lang::En ? t.en : t.de;
    }
    return key;
}

void addLine(Screen& s, size_t rows, const std::string& line) {
    if (s.count < rows && s.count < Screen::kMaxLines) s.lines[s.count++] = line;
}

}  // namespace

bool Screen::operator==(const Screen& o) const {
    if (count != o.count || firstBig != o.firstBig || icon != o.icon || status != o.status || idle != o.idle) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        if (lines[i] != o.lines[i]) return false;
    }
    return true;
}

Screen buildScreen(const View& v) {
    Screen s;
    const Lang l = v.lang;
    const size_t rows = v.rows >= 4 ? 4 : 2;
    auto add = [&s, rows](const std::string& line) { addLine(s, rows, line); };
    s.status = tr(l, v.hostConnected ? "host" : "nohost");

    switch (v.phase) {
        case Phase::Boot:
            add("RETROVERSE");
            add("Reader " + v.fwVersion);
            s.firstBig = true;
            s.status.clear();
            break;

        case Phase::Idle:
            s.icon = Icon::CardAnimation;
            s.idle = true;
            if (rows == 2 && !v.hostConnected) {
                // No status bar on the small display: the warning takes line 2.
                add(std::string(tr(l, "place1")) + " " + tr(l, "place2"));
                add(tr(l, "nohost"));
            } else {
                add(tr(l, "place1"));
                add(tr(l, "place2"));
                s.firstBig = true;
            }
            break;

        case Phase::Card:
            switch (v.format) {
                case CardFormat::Retroverse:
                case CardFormat::Legacy:
                    add(v.name);
                    s.firstBig = true;
                    if (v.hostLineCount == 0) {
                        add(tr(l, "detected"));
                    } else {
                        // `show` lines usually start with the name; do not repeat it.
                        size_t first = (v.hostLines[0] == v.name) ? 1 : 0;
                        for (size_t i = first; i < v.hostLineCount; ++i) add(v.hostLines[i]);
                        if (s.count == 1) add(tr(l, "detected"));
                    }
                    break;
                case CardFormat::Blank:
                    add(tr(l, "new1"));
                    add(tr(l, "new2"));
                    s.firstBig = true;
                    break;
                case CardFormat::Corrupt:
                case CardFormat::Unreadable:
                    add(tr(l, "bad1"));
                    add(tr(l, "bad2"));
                    break;
                case CardFormat::Unsupported:
                    add(tr(l, "unsup1"));
                    add(tr(l, "unsup2"));
                    break;
            }
            break;

        case Phase::Writing:
            add(tr(l, "writing1"));
            add(tr(l, "writing2"));
            break;

        case Phase::WriteOk:
            add(tr(l, "saved"));
            add(v.name);
            s.firstBig = true;
            s.icon = Icon::Ok;
            break;

        case Phase::WriteFailed: {
            add(tr(l, "failed"));
            const std::string key = "err_" + v.error;
            add(tr(l, key.c_str()));
            s.firstBig = true;
            s.icon = Icon::Error;
            break;
        }

        case Phase::ReaderMissing:
            add(tr(l, "rfid1"));
            add(tr(l, "rfid2"));
            s.icon = Icon::Error;
            break;
    }
    return s;
}

}  // namespace core
