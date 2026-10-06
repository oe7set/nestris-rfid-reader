# Changelog

The section of a version is the text of its GitHub release and what the
updaters (Retroverse Terminal, nestris-station) show before flashing
(`.github/scripts/release_notes.py`).

## 1.0.0

First release of the card reader firmware (ESP32 DevKit, RC522, SSD1306):
protocol v2 over USB serial, the reader detects cards placed and removed
itself, name on the card with a checksum, greeting on the display,
settings stored in the reader.
