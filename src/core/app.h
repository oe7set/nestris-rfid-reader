// Firmware behaviour, independent of the hardware: card events, commands,
// writes, heartbeat, display state. The hardware is reached through the
// small interfaces below (src/hw implements them; tests use fakes).
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "core/card_format.h"
#include "core/presence.h"
#include "core/protocol.h"
#include "core/screens.h"
#include "core/settings.h"
#include "core/uid.h"

namespace core {

enum class WriteStatus : uint8_t { Ok, NoCard, Auth, Write, Verify };

class IRfid {
public:
    virtual ~IRfid() = default;
    // One presence cycle: wake + select + halt. True if a card answered.
    virtual bool detect(Uid& uid, uint8_t& sak) = 0;
    // Read blocks 4 and 5 of the card `uid`. False on any error.
    virtual bool readBlocks(const Uid& uid, uint8_t block4[kBlockSize], uint8_t block5[kBlockSize]) = 0;
    // Write blocks 4, 5 (and zero block 6), then read back and compare.
    virtual WriteStatus writeBlocks(const Uid& uid, const uint8_t block4[kBlockSize],
                                    const uint8_t block5[kBlockSize]) = 0;
    // RC522 answers (version register). `version` gets the register value.
    virtual bool healthy(uint8_t& version) = 0;
    virtual void reinit() = 0;
};

class IDisplay {
public:
    virtual ~IDisplay() = default;
    // Apply settings; returns the display actually found (None if absent).
    virtual DisplayKind configure(DisplayKind wanted, bool flip, uint8_t brightness) = 0;
    virtual void show(const Screen& screen, uint32_t nowMs) = 0;
    // Animations and burn-in protection; called every loop.
    virtual void tick(uint32_t nowMs) = 0;
};

class ILink {
public:
    virtual ~ILink() = default;
    virtual void send(const std::string& line) = 0;
};

class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void saveSettings(const Settings& settings) = 0;
    virtual void restart() = 0;
};

class App {
public:
    static constexpr uint32_t kCycleMs = 50;
    static constexpr uint32_t kStatusIntervalMs = 2000;
    static constexpr uint32_t kHostTimeoutMs = 10000;
    static constexpr uint32_t kHealthIntervalMs = 5000;
    static constexpr uint32_t kTransientScreenMs = 3000;

    App(IRfid& rfid, IDisplay& display, ILink& link, ISystem& sys, DeviceInfo device, Settings settings);

    void begin(uint32_t nowMs);
    // One RFID cycle (call every kCycleMs) plus timers.
    void cycle(uint32_t nowMs);
    // A complete line received from the host (without the newline).
    void handleLine(const std::string& line, uint32_t nowMs);
    // The line reader dropped an over-long line.
    void lineTooLong(uint32_t nowMs);

    // For tests and diagnostics.
    const Settings& settings() const { return settings_; }
    bool hasCard() const { return card_.has_value(); }
    const CardInfo* card() const { return card_ ? &*card_ : nullptr; }
    bool writePending() const { return pending_.has_value(); }
    bool hostConnected(uint32_t nowMs) const;
    const Screen& lastScreen() const { return screen_; }

private:
    struct PendingWrite {
        std::optional<int32_t> id;
        std::string name;
        std::optional<Uid> uid;
        uint32_t deadlineMs;
        bool wrongCardSeen = false;
    };

    void onCardPresent(const Uid& uid, uint8_t sak);
    void onCardRemoved(const Uid& uid);
    void tryWrite(uint32_t nowMs);
    void finishWrite(const char* error, const std::string& detail, uint32_t nowMs);
    void checkHealth(uint32_t nowMs);
    void applyConfig(const ConfigChange& change);
    void sendHello(std::optional<int32_t> id);
    void log(const char* level, const std::string& text);
    void render(uint32_t nowMs);
    ReaderState readerState() const;

    IRfid& rfid_;
    IDisplay& display_;
    ILink& link_;
    ISystem& sys_;
    DeviceInfo device_;
    Settings settings_;
    DisplayKind attached_ = DisplayKind::None;

    Presence presence_;
    std::optional<CardInfo> card_;
    uint8_t cardSak_ = 0;
    std::optional<PendingWrite> pending_;

    std::string showLines_[kMaxShowLines];
    size_t showCount_ = 0;
    uint32_t showUntilMs_ = 0;  // 0 = until the card leaves

    Phase transient_ = Phase::Idle;  // WriteOk / WriteFailed shown for a while
    uint32_t transientUntilMs_ = 0;
    std::string transientError_;
    std::string transientName_;

    bool readerOk_ = true;
    uint8_t chipVersion_ = 0;
    uint32_t lastHealthMs_ = 0;
    uint32_t lastStatusMs_ = 0;
    uint32_t lastHostMs_ = 0;
    bool everHeardHost_ = false;
    bool booting_ = true;
    uint32_t bootUntilMs_ = 0;
    uint32_t startMs_ = 0;
    Screen screen_;
    bool screenShown_ = false;
};

}  // namespace core
