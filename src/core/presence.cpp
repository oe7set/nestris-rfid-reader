#include "core/presence.h"

namespace core {

PresenceEvents Presence::feed(const Uid* seen, uint32_t nowMs) {
    PresenceEvents ev;

    if (seen != nullptr && hasCard_ && *seen == card_) {
        // The current card is still there.
        lastSeenMs_ = nowMs;
        candidateCount_ = 0;
        return ev;
    }

    if (seen != nullptr) {
        // A card other than the current one (or the first card).
        if (candidateCount_ > 0 && *seen == candidate_) {
            ++candidateCount_;
        } else {
            candidate_ = *seen;
            candidateCount_ = 1;
        }
        if (candidateCount_ >= confirmCycles_) {
            if (hasCard_) {
                ev.removed = true;
                ev.removedUid = card_;
            }
            hasCard_ = true;
            card_ = candidate_;
            lastSeenMs_ = nowMs;
            candidateCount_ = 0;
            ev.present = true;
            ev.presentUid = card_;
        }
        return ev;
    }

    // Nothing answered this cycle.
    candidateCount_ = 0;
    // Unsigned subtraction stays correct across the millis() wrap-around.
    if (hasCard_ && static_cast<uint32_t>(nowMs - lastSeenMs_) >= removedAfterMs_) {
        ev.removed = true;
        ev.removedUid = card_;
        hasCard_ = false;
        card_ = Uid();
    }
    return ev;
}

}  // namespace core
