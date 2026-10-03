#include <ArduinoJson.h>
#include <unity.h>

#include "core/protocol.h"
#include "core/screens.h"

using namespace core;

void setUp() {}
void tearDown() {}

static JsonDocument parse(const std::string& s) {
    JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc, s) == DeserializationError::Ok);
    return doc;
}

static void test_write_command() {
    Command c = parseCommand(R"({"type":"write","id":7,"name":"Erv","uid":"04a1b2c3","timeout_ms":5000})");
    TEST_ASSERT_EQUAL(CmdType::Write, c.type);
    TEST_ASSERT_EQUAL_INT32(7, *c.id);
    TEST_ASSERT_EQUAL_STRING("Erv", c.name.c_str());
    TEST_ASSERT_EQUAL_STRING("04A1B2C3", c.uid->hex().c_str());
    TEST_ASSERT_EQUAL_UINT32(5000, c.timeoutMs);
    c = parseCommand(R"({"type":"write","name":"Erv"})");
    TEST_ASSERT_EQUAL_UINT32(kDefaultWriteTimeoutMs, c.timeoutMs);
    TEST_ASSERT_FALSE(c.id.has_value());
}

static void test_bad_requests_name_the_field() {
    struct Case { const char* line; const char* detail; } cases[] = {
        {R"({"type":"write","id":1})", "name"},
        {R"({"type":"write","id":1,"name":"way too long name"})", "name"},
        {R"({"type":"write","id":1,"name":"Erv","uid":"xyz"})", "uid"},
        {R"({"type":"write","id":1,"name":"Erv","timeout_ms":5})", "timeout_ms"},
        {R"({"type":"show","id":1,"lines":[]})", "lines"},
        {R"({"type":"show","id":1,"lines":[1]})", "lines"},
        {R"({"type":"config","id":1,"display":"96x16"})", "display"},
        {R"({"type":"config","id":1,"removed_after_ms":10})", "removed_after_ms"},
        {R"({"type":"config","id":1,"flip":"yes"})", "flip"},
        {R"({"id":1})", "type"},
    };
    for (const Case& k : cases) {
        Command c = parseCommand(k.line);
        TEST_ASSERT_EQUAL_MESSAGE(CmdType::Invalid, c.type, k.line);
        TEST_ASSERT_EQUAL_STRING_MESSAGE("bad_request", c.error.c_str(), k.line);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(k.detail, c.detail.c_str(), k.line);
    }
    TEST_ASSERT_EQUAL_STRING("bad_json", parseCommand("hello").error.c_str());
    TEST_ASSERT_EQUAL_STRING("bad_json", parseCommand("[1,2]").error.c_str());
    TEST_ASSERT_EQUAL_STRING("line_too_long", parseCommand(std::string(600, ' ')).error.c_str());
}

static void test_show_lines_are_clipped_to_ascii() {
    Command c = parseCommand(R"({"type":"show","lines":["Erv","Bestwert 159.867 Punkte heute","Jürgen"],"ttl_ms":3000})");
    TEST_ASSERT_EQUAL(CmdType::Show, c.type);
    TEST_ASSERT_EQUAL(3, c.lineCount);
    TEST_ASSERT_EQUAL_STRING("Bestwert 159.867 Punk", c.lines[1].c_str());
    TEST_ASSERT_EQUAL_STRING("J?rgen", c.lines[2].c_str());
    TEST_ASSERT_EQUAL_UINT32(3000, c.ttlMs);
}

static void test_config_and_unknown() {
    Command c = parseCommand(R"({"type":"config","display":"128x64","lang":"en","brightness":10,"flip":true})");
    TEST_ASSERT_EQUAL(CmdType::Config, c.type);
    TEST_ASSERT_EQUAL(DisplayKind::Oled128x64, *c.config.display);
    TEST_ASSERT_EQUAL(Lang::En, *c.config.lang);
    TEST_ASSERT_EQUAL_UINT8(10, *c.config.brightness);
    TEST_ASSERT_TRUE(*c.config.flip);
    TEST_ASSERT_FALSE(c.config.debug.has_value());
    c = parseCommand(R"({"type":"dance","id":3})");
    TEST_ASSERT_EQUAL(CmdType::Unknown, c.type);
    TEST_ASSERT_EQUAL_STRING("dance", c.detail.c_str());
}

static void test_messages() {
    CardInfo card{Uid(reinterpret_cast<const uint8_t*>("\x04\xA1\xB2\xC3"), 4), CardFormat::Retroverse, "Erv"};
    JsonDocument d = parse(msgCardPresent(card));
    TEST_ASSERT_EQUAL_STRING("card", d["type"]);
    TEST_ASSERT_EQUAL_STRING("present", d["state"]);
    TEST_ASSERT_EQUAL_STRING("04A1B2C3", d["uid"]);
    TEST_ASSERT_EQUAL_STRING("Erv", d["name"]);
    TEST_ASSERT_EQUAL_STRING("retroverse", d["format"]);

    card.format = CardFormat::Blank;
    card.name.clear();
    d = parse(msgCardPresent(card));
    TEST_ASSERT_TRUE(d["name"].isNull());

    d = parse(msgHello({"1.0.0", "2026-10-04T00:00:00Z", "esp32dev", "A4CF12B3C4D5"},
                       ReaderState{true, 0x92, DisplayKind::Oled128x32, Lang::De}, nullptr, 3));
    TEST_ASSERT_EQUAL_INT(2, d["proto"]);
    TEST_ASSERT_EQUAL_STRING("0x92", d["chip"]);
    TEST_ASSERT_EQUAL_STRING("128x32", d["display"]);
    TEST_ASSERT_TRUE(d["card"].isNull());
    TEST_ASSERT_EQUAL_INT(3, d["id"]);

    d = parse(msgResultError(std::nullopt, "bad_json"));
    TEST_ASSERT_TRUE(d["id"].isNull());
    TEST_ASSERT_FALSE(d["ok"]);
    TEST_ASSERT_FALSE(d["detail"].is<const char*>());
}

static void test_screens_fit_small_display() {
    View v;
    v.phase = Phase::Idle;
    v.rows = 2;
    v.hostConnected = false;
    Screen s = buildScreen(v);
    TEST_ASSERT_EQUAL(2, s.count);
    TEST_ASSERT_EQUAL_STRING("PC getrennt", s.lines[1].c_str());

    v.phase = Phase::Card;
    v.format = CardFormat::Retroverse;
    v.name = "Erv";
    v.hostLines[0] = "Erv";
    v.hostLines[1] = "Bestwert 159.867";
    v.hostLines[2] = "Platz 3";
    v.hostLineCount = 3;
    s = buildScreen(v);
    TEST_ASSERT_EQUAL(2, s.count);  // name + first host line that is not the name
    TEST_ASSERT_EQUAL_STRING("Erv", s.lines[0].c_str());
    TEST_ASSERT_EQUAL_STRING("Bestwert 159.867", s.lines[1].c_str());
    v.rows = 4;
    TEST_ASSERT_EQUAL(3, buildScreen(v).count);

    v.lang = Lang::En;
    v.phase = Phase::WriteFailed;
    v.error = "timeout";
    s = buildScreen(v);
    TEST_ASSERT_EQUAL_STRING("no card", s.lines[1].c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_write_command);
    RUN_TEST(test_bad_requests_name_the_field);
    RUN_TEST(test_show_lines_are_clipped_to_ascii);
    RUN_TEST(test_config_and_unknown);
    RUN_TEST(test_messages);
    RUN_TEST(test_screens_fit_small_display);
    return UNITY_END();
}
