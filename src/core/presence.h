// Card presence state machine (see docs/ARCHITECTURE.md#card-presence).
//
// The hardware layer polls the RC522 every cycle (WUPA + select + HLTA) and
// feeds the result here: the UID of the card that answered, or nothing.
// A new card is reported after `confirmCycles` consecutive sightings; the
// current card is reported removed after it was not seen for `removedAfterMs`.
#pragma once

#include <cstdint>

#include "core/uid.h"

namespace core {

struct PresenceEvents {
    bool removed = false;
    Uid removedUid;
    bool present = false;
    Uid presentUid;
};

class Presence {
public:
    explicit Presence(uint32_t removedAfterMs = 300, uint8_t confirmCycles = 2)
        : removedAfterMs_(removedAfterMs), confirmCycles_(confirmCycles ? confirmCycles : 1) {}

    void setRemovedAfter(uint32_t ms) { removedAfterMs_ = ms; }
    uint32_t removedAfter() const { return removedAfterMs_; }

    // One detection cycle. `seen` is nullptr when no card answered.
    PresenceEvents feed(const Uid* seen, uint32_t nowMs);

    bool hasCard() const { return hasCard_; }
    const Uid& card() const { return card_; }

    // Forget everything without events (e.g. after the RC522 was re-initialised).
    void reset() {
        hasCard_ = false;
        card_ = Uid();
        candidate_ = Uid();
        candidateCount_ = 0;
    }

private:
    uint32_t removedAfterMs_;
    uint8_t confirmCycles_;
    bool hasCard_ = false;
    Uid card_;
    uint32_t lastSeenMs_ = 0;
    Uid candidate_;
    uint8_t candidateCount_ = 0;
};

}  // namespace core
