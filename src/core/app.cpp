#include "core/app.h"

#include <utility>

namespace core {

namespace {

// True once `now` reached `deadline` (correct across the millis() wrap-around).
bool reached(uint32_t now, uint32_t deadline) { return static_cast<int32_t>(now - deadline) >= 0; }

constexpr uint32_t kBootScreenMs = 1500;

}  // namespace

App::App(IRfid& rfid, IDisplay& display, ILink& link, ISystem& sys, DeviceInfo device, Settings settings)
    : rfid_(rfid),
      display_(display),
      link_(link),
      sys_(sys),
      device_(std::move(device)),
      settings_(settings),
      presence_(settings.removedAfterMs) {}

void App::begin(uint32_t nowMs) {
    startMs_ = nowMs;
    attached_ = display_.configure(settings_.display, settings_.flip, settings_.brightness);
    readerOk_ = rfid_.healthy(chipVersion_);
    if (!readerOk_) {
        rfid_.reinit();
        readerOk_ = rfid_.healthy(chipVersion_);
    }
    lastHealthMs_ = nowMs;
    lastStatusMs_ = nowMs;
    booting_ = true;
    bootUntilMs_ = nowMs + kBootScreenMs;
    sendHello(std::nullopt);
    if (!readerOk_) log("error", "RC522 not answering");
    render(nowMs);
}

bool App::hostConnected(uint32_t nowMs) const {
    return everHeardHost_ && static_cast<uint32_t>(nowMs - lastHostMs_) < kHostTimeoutMs;
}

ReaderState App::readerState() const {
    ReaderState st;
    st.readerOk = readerOk_;
    st.chipVersion = chipVersion_;
    st.display = attached_;
    st.lang = settings_.lang;
    return st;
}

void App::sendHello(std::optional<int32_t> id) { link_.send(msgHello(device_, readerState(), card(), id)); }

void App::log(const char* level, const std::string& text) {
    if (std::string(level) == "debug" && !settings_.debug) return;
    link_.send(msgLog(level, text));
}

// ---------------------------------------------------------------- cycle

void App::cycle(uint32_t nowMs) {
    if (booting_ && reached(nowMs, bootUntilMs_)) booting_ = false;

    if (readerOk_) {
        Uid uid;
        uint8_t sak = 0;
        const bool seen = rfid_.detect(uid, sak);
        const PresenceEvents ev = presence_.feed(seen ? &uid : nullptr, nowMs);
        if (ev.removed) onCardRemoved(ev.removedUid);
        if (ev.present) onCardPresent(ev.presentUid, sak);
    }

    tryWrite(nowMs);

    if (static_cast<uint32_t>(nowMs - lastHealthMs_) >= kHealthIntervalMs) checkHealth(nowMs);

    if (static_cast<uint32_t>(nowMs - lastStatusMs_) >= kStatusIntervalMs) {
        lastStatusMs_ = nowMs;
        link_.send(msgStatus(card(), readerOk_, (nowMs - startMs_) / 1000, hostConnected(nowMs)));
    }

    if (showUntilMs_ != 0 && reached(nowMs, showUntilMs_)) {
        showCount_ = 0;
        showUntilMs_ = 0;
    }

    render(nowMs);
    display_.tick(nowMs);
}

void App::onCardPresent(const Uid& uid, uint8_t sak) {
    CardInfo info;
    info.uid = uid;
    if (!isMifareClassic(sak)) {
        info.format = CardFormat::Unsupported;
    } else {
        uint8_t b4[kBlockSize];
        uint8_t b5[kBlockSize];
        if (rfid_.readBlocks(uid, b4, b5)) {
            DecodedCard decoded = decodeCard(b4, b5);
            info.format = decoded.format;
            info.name = decoded.name;
        } else {
            info.format = CardFormat::Unreadable;
        }
    }
    card_ = info;
    cardSak_ = sak;
    showCount_ = 0;
    showUntilMs_ = 0;
    link_.send(msgCardPresent(info));
}

void App::onCardRemoved(const Uid& uid) {
    card_.reset();
    showCount_ = 0;
    showUntilMs_ = 0;
    link_.send(msgCardRemoved(uid));
}

void App::checkHealth(uint32_t nowMs) {
    lastHealthMs_ = nowMs;
    bool ok = rfid_.healthy(chipVersion_);
    if (!ok) {
        rfid_.reinit();
        ok = rfid_.healthy(chipVersion_);
    }
    if (ok == readerOk_) return;
    readerOk_ = ok;
    if (!ok) {
        log("warn", "RC522 not answering, re-initialising");
        if (card_) onCardRemoved(card_->uid);
        presence_.reset();
    } else {
        log("info", "RC522 is back");
    }
}

// ---------------------------------------------------------------- writes

void App::tryWrite(uint32_t nowMs) {
    if (!pending_) return;
    PendingWrite& w = *pending_;

    if (reached(nowMs, w.deadlineMs)) {
        if (w.wrongCardSeen) {
            finishWrite("wrong_card", "the card on the reader is not " + w.uid->hex(), nowMs);
        } else {
            finishWrite("timeout", "no card within the timeout", nowMs);
        }
        return;
    }
    if (!card_) return;
    if (w.uid && *w.uid != card_->uid) {
        w.wrongCardSeen = true;
        return;
    }
    if (card_->format == CardFormat::Unsupported) {
        finishWrite("unsupported_card", "", nowMs);
        return;
    }

    uint8_t b4[kBlockSize];
    uint8_t b5[kBlockSize];
    encodeCard(w.name, b4, b5);  // the name was validated when the command arrived
    const WriteStatus st = rfid_.writeBlocks(card_->uid, b4, b5);

    switch (st) {
        case WriteStatus::Ok: {
            card_->name = w.name;
            card_->format = CardFormat::Retroverse;
            link_.send(msgResultOkCard(w.id, card_->uid, w.name));
            link_.send(msgCardPresent(*card_));
            transient_ = Phase::WriteOk;
            transientName_ = w.name;
            transientUntilMs_ = nowMs + kTransientScreenMs;
            pending_.reset();
            return;
        }
        case WriteStatus::NoCard:
            return;  // the card left before the write started: keep waiting
        case WriteStatus::Auth:
            finishWrite("auth", "authentication with the default key failed", nowMs);
            break;
        case WriteStatus::Write:
            finishWrite("write", "the card did not accept the write", nowMs);
            break;
        case WriteStatus::Verify:
            finishWrite("verify", "read-back differs from what was written", nowMs);
            break;
    }

    // After a failed write the card may hold something else now: re-read it.
    if (card_) {
        uint8_t r4[kBlockSize];
        uint8_t r5[kBlockSize];
        CardInfo info = *card_;
        if (rfid_.readBlocks(info.uid, r4, r5)) {
            DecodedCard d = decodeCard(r4, r5);
            info.format = d.format;
            info.name = d.name;
        } else {
            info.format = CardFormat::Unreadable;
            info.name.clear();
        }
        if (info.format != card_->format || info.name != card_->name) {
            card_ = info;
            link_.send(msgCardPresent(info));
        }
    }
}

void App::finishWrite(const char* error, const std::string& detail, uint32_t nowMs) {
    link_.send(msgResultError(pending_->id, error, detail));
    pending_.reset();
    transient_ = Phase::WriteFailed;
    transientError_ = error;
    transientUntilMs_ = nowMs + kTransientScreenMs;
}

// ---------------------------------------------------------------- commands

void App::lineTooLong(uint32_t nowMs) {
    lastHostMs_ = nowMs;
    everHeardHost_ = true;
    link_.send(msgResultError(std::nullopt, "line_too_long"));
}

void App::handleLine(const std::string& line, uint32_t nowMs) {
    lastHostMs_ = nowMs;
    everHeardHost_ = true;
    const Command cmd = parseCommand(line);

    switch (cmd.type) {
        case CmdType::Invalid:
            link_.send(msgResultError(cmd.id, cmd.error.c_str(), cmd.detail));
            break;

        case CmdType::Hello:
            sendHello(cmd.id);
            break;

        case CmdType::Ping:
            link_.send(msgResultOk(cmd.id));
            break;

        case CmdType::Write:
            if (pending_) {
                link_.send(msgResultError(cmd.id, "busy", "another write is pending"));
                break;
            }
            pending_ = PendingWrite{cmd.id, cmd.name, cmd.uid, nowMs + cmd.timeoutMs};
            transientUntilMs_ = 0;
            tryWrite(nowMs);  // the card may already be on the reader
            break;

        case CmdType::Show:
            if (cmd.uid && (!card_ || card_->uid != *cmd.uid)) {
                link_.send(msgResultError(cmd.id, "wrong_card", "card " + cmd.uid->hex() + " is not present"));
                break;
            }
            if (!card_) {
                link_.send(msgResultError(cmd.id, "no_card"));
                break;
            }
            for (size_t i = 0; i < cmd.lineCount; ++i) showLines_[i] = cmd.lines[i];
            showCount_ = cmd.lineCount;
            showUntilMs_ = cmd.ttlMs ? nowMs + cmd.ttlMs : 0;
            if (showUntilMs_ == 0 && cmd.ttlMs) showUntilMs_ = 1;  // avoid the "forever" marker
            link_.send(msgResultOk(cmd.id));
            break;

        case CmdType::Cancel:
            if (pending_) {
                link_.send(msgResultError(pending_->id, "cancelled"));
                pending_.reset();
            }
            link_.send(msgResultOk(cmd.id));
            break;

        case CmdType::Config:
            applyConfig(cmd.config);
            link_.send(msgResultOk(cmd.id));
            sendHello(std::nullopt);
            break;

        case CmdType::Reboot:
            link_.send(msgResultOk(cmd.id));
            sys_.restart();
            break;

        case CmdType::Unknown:
            link_.send(msgResultError(cmd.id, "unknown_type", cmd.detail));
            break;
    }
    render(nowMs);
}

void App::applyConfig(const ConfigChange& change) {
    bool displayChanged = false;
    if (change.display && *change.display != settings_.display) {
        settings_.display = *change.display;
        displayChanged = true;
    }
    if (change.flip && *change.flip != settings_.flip) {
        settings_.flip = *change.flip;
        displayChanged = true;
    }
    if (change.brightness && *change.brightness != settings_.brightness) {
        settings_.brightness = *change.brightness;
        displayChanged = true;
    }
    if (change.lang) settings_.lang = *change.lang;
    if (change.debug) settings_.debug = *change.debug;
    if (change.removedAfterMs) {
        settings_.removedAfterMs = *change.removedAfterMs;
        presence_.setRemovedAfter(settings_.removedAfterMs);
    }
    if (displayChanged) {
        attached_ = display_.configure(settings_.display, settings_.flip, settings_.brightness);
        screenShown_ = false;  // redraw on the new display
    }
    sys_.saveSettings(settings_);
}

// ---------------------------------------------------------------- display

void App::render(uint32_t nowMs) {
    View v;
    v.lang = settings_.lang;
    v.rows = attached_ == DisplayKind::Oled128x64 ? 4 : 2;
    v.hostConnected = hostConnected(nowMs);
    v.fwVersion = device_.fw;

    const bool transientActive = transientUntilMs_ != 0 && !reached(nowMs, transientUntilMs_);
    if (!transientActive) transientUntilMs_ = 0;

    if (booting_) {
        v.phase = Phase::Boot;
    } else if (!readerOk_) {
        v.phase = Phase::ReaderMissing;
    } else if (pending_) {
        v.phase = Phase::Writing;
    } else if (transientActive) {
        v.phase = transient_;
        v.name = transientName_;
        v.error = transientError_;
    } else if (card_) {
        v.phase = Phase::Card;
        v.format = card_->format;
        v.name = card_->name;
        for (size_t i = 0; i < showCount_; ++i) v.hostLines[i] = showLines_[i];
        v.hostLineCount = showCount_;
    } else {
        v.phase = Phase::Idle;
    }

    const Screen s = buildScreen(v);
    if (!screenShown_ || s != screen_) {
        screen_ = s;
        screenShown_ = true;
        if (attached_ != DisplayKind::None) display_.show(s, nowMs);
    }
}

}  // namespace core
