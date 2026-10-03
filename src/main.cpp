// nestris-rfid-reader: wiring of the hardware to core::App.
// Pins: docs/ARCHITECTURE.md#hardware (the v1 reader wiring).

#include <Arduino.h>
#include <esp_mac.h>
#include <esp_task_wdt.h>

#include "core/app.h"
#include "hw/oled.h"
#include "hw/rfid_rc522.h"
#include "hw/serial_link.h"
#include "hw/settings_store.h"

#ifndef FW_VERSION
#define FW_VERSION "0.0.0-dev"
#endif
#ifndef FW_BUILD
#define FW_BUILD ""
#endif

namespace {

constexpr uint8_t kRc522Ss = 5;
constexpr uint8_t kRc522Rst = 4;
constexpr uint8_t kOledSda = 21;
constexpr uint8_t kOledScl = 22;
constexpr uint32_t kBaud = 115200;
constexpr uint32_t kWatchdogS = 5;

class System : public core::ISystem {
public:
    void saveSettings(const core::Settings& settings) override { hw::saveSettings(settings); }
    void restart() override {
        Serial.flush();
        delay(50);
        ESP.restart();
    }
};

std::string macSerial() {
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);  // factory MAC, unique per chip
    char buf[13];
    snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buf;
}

hw::SerialLink serialLink;
hw::RfidRc522 rfid(kRc522Ss, kRc522Rst);
hw::Oled oled(kOledSda, kOledScl);
System sys;
core::App* app = nullptr;
uint32_t lastCycle = 0;

}  // namespace

void setup() {
    serialLink.begin(kBaud);
    rfid.begin();

    core::DeviceInfo device{FW_VERSION, FW_BUILD, "esp32dev", macSerial()};
    app = new core::App(rfid, oled, serialLink, sys, device, hw::loadSettings());
    app->begin(millis());

    esp_task_wdt_init(kWatchdogS, true);  // reboot if the loop hangs
    esp_task_wdt_add(nullptr);
}

void loop() {
    const uint32_t now = millis();

    std::string line;
    bool tooLong = false;
    while (serialLink.poll(line, tooLong)) app->handleLine(line, now);
    if (tooLong) app->lineTooLong(now);

    if (now - lastCycle >= core::App::kCycleMs) {
        lastCycle = now;
        app->cycle(now);
    }

    esp_task_wdt_reset();
    delay(2);  // yield to the idle task
}
