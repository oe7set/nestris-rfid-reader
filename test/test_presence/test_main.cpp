#include <unity.h>

#include "core/presence.h"

using core::Presence;
using core::Uid;

static const uint8_t A[] = {0x04, 0xA1, 0xB2, 0xC3};
static const uint8_t B[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

void setUp() {}
void tearDown() {}

static void test_new_card_needs_two_sightings() {
    Presence p(300, 2);
    Uid a(A, 4);
    TEST_ASSERT_FALSE(p.feed(&a, 0).present);
    auto ev = p.feed(&a, 50);
    TEST_ASSERT_TRUE(ev.present);
    TEST_ASSERT_TRUE(ev.presentUid == a);
    TEST_ASSERT_TRUE(p.hasCard());
}

static void test_single_glitch_is_ignored() {
    Presence p(300, 2);
    Uid a(A, 4);
    p.feed(&a, 0);
    p.feed(nullptr, 50);
    TEST_ASSERT_FALSE(p.feed(&a, 100).present);  // the count restarted
    TEST_ASSERT_TRUE(p.feed(&a, 150).present);
}

static void test_removed_after_timeout_only() {
    Presence p(300, 2);
    Uid a(A, 4);
    p.feed(&a, 0);
    p.feed(&a, 50);
    for (uint32_t t = 100; t < 350; t += 50) TEST_ASSERT_FALSE(p.feed(nullptr, t).removed);
    auto ev = p.feed(nullptr, 350);
    TEST_ASSERT_TRUE(ev.removed);
    TEST_ASSERT_TRUE(ev.removedUid == a);
    TEST_ASSERT_FALSE(p.hasCard());
}

static void test_short_lift_produces_no_events() {
    Presence p(300, 2);
    Uid a(A, 4);
    p.feed(&a, 0);
    p.feed(&a, 50);
    p.feed(nullptr, 100);
    p.feed(nullptr, 150);
    auto ev = p.feed(&a, 200);
    TEST_ASSERT_FALSE(ev.present);
    TEST_ASSERT_FALSE(ev.removed);
    TEST_ASSERT_FALSE(p.feed(nullptr, 450).removed);  // last seen at 200
    TEST_ASSERT_TRUE(p.feed(nullptr, 500).removed);
}

static void test_card_swap_reports_removed_then_present() {
    Presence p(300, 2);
    Uid a(A, 4);
    Uid b(B, 7);
    p.feed(&a, 0);
    p.feed(&a, 50);
    TEST_ASSERT_FALSE(p.feed(&b, 100).present);
    auto ev = p.feed(&b, 150);
    TEST_ASSERT_TRUE(ev.removed);
    TEST_ASSERT_TRUE(ev.removedUid == a);
    TEST_ASSERT_TRUE(ev.present);
    TEST_ASSERT_TRUE(ev.presentUid == b);
}

static void test_millis_wraparound() {
    Presence p(300, 2);
    Uid a(A, 4);
    const uint32_t t0 = 0xFFFFFF00u;
    p.feed(&a, t0);
    p.feed(&a, t0 + 50);
    TEST_ASSERT_FALSE(p.feed(nullptr, t0 + 200).removed);
    TEST_ASSERT_TRUE(p.feed(nullptr, t0 + 50 + 300).removed);  // wrapped past zero
}

static void test_uid_hex_and_parse() {
    Uid b(B, 7);
    TEST_ASSERT_EQUAL_STRING("11223344556677", b.hex().c_str());
    Uid parsed;
    TEST_ASSERT_TRUE(Uid::parse("04a1b2c3", parsed));
    TEST_ASSERT_EQUAL_STRING("04A1B2C3", parsed.hex().c_str());
    TEST_ASSERT_FALSE(Uid::parse("04A1B2", parsed));
    TEST_ASSERT_FALSE(Uid::parse("04A1B2CZ", parsed));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_new_card_needs_two_sightings);
    RUN_TEST(test_single_glitch_is_ignored);
    RUN_TEST(test_removed_after_timeout_only);
    RUN_TEST(test_short_lift_produces_no_events);
    RUN_TEST(test_card_swap_reports_removed_then_present);
    RUN_TEST(test_millis_wraparound);
    RUN_TEST(test_uid_hex_and_parse);
    return UNITY_END();
}
