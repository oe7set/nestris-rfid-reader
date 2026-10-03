// Settings in NVS (Preferences namespace "reader"); survive firmware updates.
#pragma once

#include "core/settings.h"

namespace hw {

core::Settings loadSettings();
void saveSettings(const core::Settings& settings);

}  // namespace hw
