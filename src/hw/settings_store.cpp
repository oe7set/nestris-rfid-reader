#include "hw/settings_store.h"

#include <Preferences.h>

namespace hw {

namespace {
constexpr const char* kNamespace = "reader";
}

core::Settings loadSettings() {
    core::Settings s;
    Preferences p;
    if (!p.begin(kNamespace, true)) return s;  // first boot: defaults
    const uint8_t display = p.getUChar("display", static_cast<uint8_t>(s.display));
    if (display <= static_cast<uint8_t>(core::DisplayKind::None)) s.display = static_cast<core::DisplayKind>(display);
    s.lang = p.getUChar("lang", 0) == 1 ? core::Lang::En : core::Lang::De;
    const uint16_t removed = p.getUShort("removed_ms", s.removedAfterMs);
    if (removed >= core::Settings::kMinRemovedAfterMs && removed <= core::Settings::kMaxRemovedAfterMs) {
        s.removedAfterMs = removed;
    }
    s.brightness = p.getUChar("brightness", s.brightness);
    s.flip = p.getBool("flip", s.flip);
    s.debug = p.getBool("debug", s.debug);
    p.end();
    return s;
}

void saveSettings(const core::Settings& s) {
    Preferences p;
    if (!p.begin(kNamespace, false)) return;
    p.putUChar("display", static_cast<uint8_t>(s.display));
    p.putUChar("lang", s.lang == core::Lang::En ? 1 : 0);
    p.putUShort("removed_ms", s.removedAfterMs);
    p.putUChar("brightness", s.brightness);
    p.putBool("flip", s.flip);
    p.putBool("debug", s.debug);
    p.end();
}

}  // namespace hw
