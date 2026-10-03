// core::App against fake hardware: the firmware's behaviour end to end.
#include <ArduinoJson.h>
#include <unity.h>

#include <cstring>
#include <optional>
#include <vector>

#include "core/app.h"

using namespace core;

// ---------------------------------------------------------------- fakes

struct FakeCard {
    Uid uid;
    uint8_t sak = 0x08;
    uint8_t b4[16] = {};
    uint8_t b5[16] = {};
};

class FakeRfid : public IRfid {
public:
    std::optional<FakeCard> card;
    bool chipOk = true;
    bool failAuth = false;
    bool corruptOnWrite = false;  // writes garbage and reports a verify error
    int writes = 0;

    bool detect(Uid& uid, uint8_t& sak) override {
        if (!chipOk || !card) return false;
        uid = card->uid;
        sak = card->sak;
        return true;
    }
    bool readBlocks(const Uid& uid, uint8_t b4[16], uint8_t b5[16]) override {
        if (!card || card->uid != uid || failAuth) return false;
        std::memcpy(b4, card->b4, 16);
        std::memcpy(b5, card->b5, 16);
        return true;
    }
    WriteStatus writeBlocks(const Uid& uid, const uint8_t b4[16], const uint8_t b5[16]) override {
        if (!card || card->uid != uid) return WriteStatus::NoCard;
        if (failAuth) return WriteStatus::Auth;
        ++writes;
        std::memcpy(card->b4, b4, 16);
        std::memcpy(card->b5, b5, 16);
        if (corruptOnWrite) {
            card->b4[0] ^= 0xFF;
            return WriteStatus::Verify;
        }
        return WriteStatus::Ok;
    }
    bool healthy(uint8_t& version) override {
        version = chipOk ? 0x92 : 0x00;
        return chipOk;
    }
    void reinit() override {}
};

class FakeDisplay : public IDisplay {
public:
    bool attached = true;
    Screen last;
    int shows = 0;
    DisplayKind configure(DisplayKind wanted, bool, uint8_t) override {
        return attached ? wanted : DisplayKind::None;
    }
    void show(const Screen& s, uint32_t) override {
        last = s;
        ++shows;
    }
    void tick(uint32_t) override {}
};

class FakeLink : public ILink {
public:
    std::vector<std::string> lines;
    void send(const std::string& line) override { lines.push_back(line); }

    // All messages of `type` (optionally with `state`) sent so far, parsed.
    std::vector<JsonDocument> of(const char* type, const char* state = nullptr) const {
        std::vector<JsonDocument> out;
        for (const std::string& l : lines) {
            JsonDocument d;
            deserializeJson(d, l);
            if (std::strcmp(d["type"] | "", type) != 0) continue;
            if (state && std::strcmp(d["state"] | "", state) != 0) continue;
            out.push_back(d);
        }
        return out;
    }
    JsonDocument lastOf(const char* type, const char* state = nullptr) const {
        auto all = of(type, state);
        TEST_ASSERT_FALSE_MESSAGE(all.empty(), type);
        return all.back();
    }
};

class FakeSystem : public ISystem {
public:
    int saves = 0;
    bool restarted = false;
    Settings saved;
    void saveSettings(const Settings& s) override {
        ++saves;
        saved = s;
    }
    void restart() override { restarted = true; }
};

// ---------------------------------------------------------------- fixture

static const uint8_t kUidA[] = {0x04, 0xA1, 0xB2, 0xC3};
static const uint8_t kUidB[] = {0x0B, 0x0B, 0x0B, 0x0B};

struct Bench {
    FakeRfid rfid;
    FakeDisplay display;
    FakeLink link;
    FakeSystem sys;
    App app;
    uint32_t now = 1000;

    Bench() : app(rfid, display, link, sys, DeviceInfo{"1.2.3", "build", "esp32dev", "A4CF12B3C4D5"}, Settings{}) {
        app.begin(now);
        run(2000);  // past the boot screen
    }

    void run(uint32_t ms) {
        for (uint32_t end = now + ms; now < end;) {
            now += App::kCycleMs;
            app.cycle(now);
        }
    }
    void send(const std::string& line) { app.handleLine(line, now); }

    void place(const uint8_t* uid, const char* legacyName = nullptr, uint8_t sak = 0x08) {
        FakeCard c;
        c.uid = Uid(uid, 4);
        c.sak = sak;
        if (legacyName) std::memcpy(c.b4, legacyName, std::strlen(legacyName));
        rfid.card = c;
        run(150);
    }
    void remove() {
        rfid.card.reset();
        run(400);
    }
};

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------- tests

static void test_boot_hello() {
    Bench b;
    JsonDocument h = b.link.lastOf("hello");
    TEST_ASSERT_EQUAL_INT(2, h["proto"]);
    TEST_ASSERT_EQUAL_STRING("1.2.3", h["fw"]);
    TEST_ASSERT_EQUAL_STRING("ok", h["reader"]);
    TEST_ASSERT_EQUAL_STRING("128x32", h["display"]);
    TEST_ASSERT_EQUAL_STRING("A4CF12B3C4D5", h["serial"]);
    // No host has spoken yet: the small display says so on line 2.
    TEST_ASSERT_EQUAL_STRING("Karte auflegen", b.display.last.lines[0].c_str());
    TEST_ASSERT_EQUAL_STRING("PC getrennt", b.display.last.lines[1].c_str());
    b.send(R"({"type":"ping","id":1})");
    TEST_ASSERT_EQUAL_STRING("Karte", b.display.last.lines[0].c_str());
    TEST_ASSERT_EQUAL_STRING("auflegen", b.display.last.lines[1].c_str());
}

static void test_place_and_remove_legacy_card() {
    Bench b;
    b.place(kUidA, "Erv");
    JsonDocument p = b.link.lastOf("card", "present");
    TEST_ASSERT_EQUAL_STRING("04A1B2C3", p["uid"]);
    TEST_ASSERT_EQUAL_STRING("Erv", p["name"]);
    TEST_ASSERT_EQUAL_STRING("legacy", p["format"]);
    TEST_ASSERT_EQUAL_STRING("Erv", b.display.last.lines[0].c_str());
    TEST_ASSERT_EQUAL(1, b.link.of("card", "present").size());  // once, not every cycle

    b.remove();
    TEST_ASSERT_EQUAL_STRING("04A1B2C3", b.link.lastOf("card", "removed")["uid"]);
    TEST_ASSERT_FALSE(b.app.hasCard());
}

static void test_write_to_blank_card_on_reader() {
    Bench b;
    b.place(kUidA);
    TEST_ASSERT_EQUAL_STRING("blank", b.link.lastOf("card", "present")["format"]);
    b.send(R"({"type":"write","id":7,"name":"Neo","uid":"04A1B2C3"})");
    JsonDocument r = b.link.lastOf("result");
    TEST_ASSERT_EQUAL_INT(7, r["id"]);
    TEST_ASSERT_TRUE(r["ok"]);
    TEST_ASSERT_EQUAL_STRING("Neo", r["name"]);
    JsonDocument p = b.link.lastOf("card", "present");
    TEST_ASSERT_EQUAL_STRING("retroverse", p["format"]);
    TEST_ASSERT_EQUAL_STRING("Neo", p["name"]);
    TEST_ASSERT_EQUAL_STRING("Gespeichert", b.display.last.lines[0].c_str());
    b.run(3500);
    TEST_ASSERT_EQUAL_STRING("Neo", b.display.last.lines[0].c_str());
    // The card now decodes as a Retroverse card.
    DecodedCard d = decodeCard(b.rfid.card->b4, b.rfid.card->b5);
    TEST_ASSERT_EQUAL(CardFormat::Retroverse, d.format);
}

static void test_write_waits_for_the_card() {
    Bench b;
    b.send(R"({"type":"write","id":1,"name":"Neo","timeout_ms":5000})");
    TEST_ASSERT_TRUE(b.app.writePending());
    TEST_ASSERT_EQUAL_STRING("Schreibe ...", b.display.last.lines[0].c_str());
    b.run(1000);
    TEST_ASSERT_EQUAL(0, b.link.of("result").size());
    b.place(kUidA);
    TEST_ASSERT_TRUE(b.link.lastOf("result")["ok"]);
    TEST_ASSERT_FALSE(b.app.writePending());
}

static void test_write_timeout_and_wrong_card() {
    Bench b;
    b.send(R"({"type":"write","id":2,"name":"Neo","timeout_ms":1000})");
    b.run(1100);
    TEST_ASSERT_EQUAL_STRING("timeout", b.link.lastOf("result")["error"]);
    TEST_ASSERT_EQUAL_STRING("Fehler", b.display.last.lines[0].c_str());

    b.place(kUidB);
    b.send(R"({"type":"write","id":3,"name":"Neo","uid":"04A1B2C3","timeout_ms":1000})");
    b.run(1100);
    JsonDocument r = b.link.lastOf("result");
    TEST_ASSERT_EQUAL_INT(3, r["id"]);
    TEST_ASSERT_EQUAL_STRING("wrong_card", r["error"]);
    TEST_ASSERT_EQUAL(0, b.rfid.writes);
}

static void test_busy_and_cancel() {
    Bench b;
    b.send(R"({"type":"write","id":4,"name":"Neo"})");
    b.send(R"({"type":"write","id":5,"name":"Two"})");
    JsonDocument r = b.link.lastOf("result");
    TEST_ASSERT_EQUAL_INT(5, r["id"]);
    TEST_ASSERT_EQUAL_STRING("busy", r["error"]);
    b.send(R"({"type":"cancel","id":6})");
    auto results = b.link.of("result");
    TEST_ASSERT_EQUAL_INT(4, results[results.size() - 2]["id"]);
    TEST_ASSERT_EQUAL_STRING("cancelled", results[results.size() - 2]["error"]);
    TEST_ASSERT_TRUE(results.back()["ok"]);
    TEST_ASSERT_FALSE(b.app.writePending());
}

static void test_failed_verify_reports_and_rereads() {
    Bench b;
    b.place(kUidA, "Erv");
    b.rfid.corruptOnWrite = true;
    b.send(R"({"type":"write","id":8,"name":"Neo"})");
    TEST_ASSERT_EQUAL_STRING("verify", b.link.lastOf("result")["error"]);
    TEST_ASSERT_EQUAL_STRING("corrupt", b.link.lastOf("card", "present")["format"]);
}

static void test_unsupported_card() {
    Bench b;
    b.place(kUidA, nullptr, 0x20);  // e.g. a phone
    TEST_ASSERT_EQUAL_STRING("unsupported", b.link.lastOf("card", "present")["format"]);
    b.send(R"({"type":"write","id":9,"name":"Neo"})");
    TEST_ASSERT_EQUAL_STRING("unsupported_card", b.link.lastOf("result")["error"]);
}

static void test_show_lines_until_card_leaves() {
    Bench b;
    b.send(R"({"type":"show","id":1,"lines":["Bestwert 1"]})");
    TEST_ASSERT_EQUAL_STRING("no_card", b.link.lastOf("result")["error"]);
    b.place(kUidA, "Erv");
    b.send(R"({"type":"show","id":2,"uid":"0B0B0B0B","lines":["x"]})");
    TEST_ASSERT_EQUAL_STRING("wrong_card", b.link.lastOf("result")["error"]);
    b.send(R"({"type":"show","id":3,"uid":"04A1B2C3","lines":["Erv","Bestwert 159.867"]})");
    TEST_ASSERT_TRUE(b.link.lastOf("result")["ok"]);
    TEST_ASSERT_EQUAL_STRING("Bestwert 159.867", b.display.last.lines[1].c_str());
    b.remove();
    b.place(kUidA, "Erv");
    TEST_ASSERT_EQUAL_STRING("Karte erkannt", b.display.last.lines[1].c_str());
}

static void test_config_is_applied_and_saved() {
    Bench b;
    b.send(R"({"type":"config","id":4,"display":"128x64","lang":"en","removed_after_ms":1000})");
    TEST_ASSERT_TRUE(b.link.lastOf("result")["ok"]);
    TEST_ASSERT_EQUAL_STRING("128x64", b.link.lastOf("hello")["display"]);
    TEST_ASSERT_EQUAL(1, b.sys.saves);
    TEST_ASSERT_EQUAL(Lang::En, b.sys.saved.lang);
    TEST_ASSERT_EQUAL_STRING("Place", b.display.last.lines[0].c_str());
    b.place(kUidA, "Erv");
    b.rfid.card.reset();
    b.run(500);
    TEST_ASSERT_TRUE(b.app.hasCard());  // removed_after_ms is now 1000
    b.run(600);
    TEST_ASSERT_FALSE(b.app.hasCard());
}

static void test_no_display_attached() {
    Bench b;
    b.display.attached = false;
    b.send(R"({"type":"config","id":1,"display":"128x64"})");
    TEST_ASSERT_EQUAL_STRING("none", b.link.lastOf("hello")["display"]);
    const int before = b.display.shows;
    b.place(kUidA, "Erv");
    TEST_ASSERT_EQUAL(before, b.display.shows);  // nothing drawn, events still sent
    TEST_ASSERT_EQUAL_STRING("Erv", b.link.lastOf("card", "present")["name"]);
}

static void test_reader_missing_and_back() {
    Bench b;
    b.place(kUidA, "Erv");
    b.rfid.chipOk = false;
    b.run(App::kHealthIntervalMs + 100);
    TEST_ASSERT_EQUAL_STRING("04A1B2C3", b.link.lastOf("card", "removed")["uid"]);
    TEST_ASSERT_EQUAL_STRING("warn", b.link.lastOf("log")["level"]);
    TEST_ASSERT_EQUAL_STRING("missing", b.link.lastOf("status")["reader"]);
    TEST_ASSERT_EQUAL_STRING("Leser-Fehler", b.display.last.lines[0].c_str());
    b.rfid.chipOk = true;
    b.run(App::kHealthIntervalMs + 100);
    TEST_ASSERT_EQUAL_STRING("info", b.link.lastOf("log")["level"]);
    TEST_ASSERT_TRUE(b.app.hasCard());  // the card is seen again
}

static void test_status_heartbeat_and_host_liveness() {
    Bench b;
    const size_t before = b.link.of("status").size();
    b.run(4000);
    TEST_ASSERT_EQUAL(before + 2, b.link.of("status").size());
    TEST_ASSERT_FALSE(b.link.lastOf("status")["host"]);
    b.send(R"({"type":"ping","id":1})");
    TEST_ASSERT_TRUE(b.link.lastOf("result")["ok"]);
    b.run(2000);
    TEST_ASSERT_TRUE(b.link.lastOf("status")["host"]);
    b.run(App::kHostTimeoutMs);
    TEST_ASSERT_FALSE(b.link.lastOf("status")["host"]);
    TEST_ASSERT_EQUAL_STRING("PC getrennt", b.display.last.lines[1].c_str());
}

static void test_reboot_and_errors() {
    Bench b;
    b.send("not json");
    JsonDocument r = b.link.lastOf("result");
    TEST_ASSERT_TRUE(r["id"].isNull());
    TEST_ASSERT_EQUAL_STRING("bad_json", r["error"]);
    b.app.lineTooLong(b.now);
    TEST_ASSERT_EQUAL_STRING("line_too_long", b.link.lastOf("result")["error"]);
    b.send(R"({"type":"fly","id":2})");
    TEST_ASSERT_EQUAL_STRING("unknown_type", b.link.lastOf("result")["error"]);
    b.send(R"({"type":"hello","id":5})");
    TEST_ASSERT_EQUAL_INT(5, b.link.lastOf("hello")["id"]);
    b.send(R"({"type":"reboot","id":6})");
    TEST_ASSERT_TRUE(b.link.lastOf("result")["ok"]);
    TEST_ASSERT_TRUE(b.sys.restarted);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_boot_hello);
    RUN_TEST(test_place_and_remove_legacy_card);
    RUN_TEST(test_write_to_blank_card_on_reader);
    RUN_TEST(test_write_waits_for_the_card);
    RUN_TEST(test_write_timeout_and_wrong_card);
    RUN_TEST(test_busy_and_cancel);
    RUN_TEST(test_failed_verify_reports_and_rereads);
    RUN_TEST(test_unsupported_card);
    RUN_TEST(test_show_lines_until_card_leaves);
    RUN_TEST(test_config_is_applied_and_saved);
    RUN_TEST(test_no_display_attached);
    RUN_TEST(test_reader_missing_and_back);
    RUN_TEST(test_status_heartbeat_and_host_liveness);
    RUN_TEST(test_reboot_and_errors);
    return UNITY_END();
}
