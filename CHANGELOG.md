# Changelog

The section of a version is the text of its GitHub release and what the
updaters (Retroverse Terminal, nestris-station) show before flashing
(`.github/scripts/release_notes.py`).

## 1.1.0

- New command `erase`: makes a card new again (name and marker zeroed,
  checked by reading back); the display shows "Geleert". Used by the clear
  mode of the Retroverse Terminal (0.3.0b3). Protocol stays v2; hosts that do
  not use `erase` are not affected.

## 1.0.0

First release of the card reader firmware (ESP32 DevKit, RC522, SSD1306):
protocol v2 over USB serial, the reader detects cards placed and removed
itself, name on the card with a checksum, greeting on the display,
settings stored in the reader.
