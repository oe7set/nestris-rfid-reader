# nestris-rfid-reader

Firmware for the Retroverse player-card reader: **ESP32 DevKit + RC522 RFID +
SSD1306 OLED (128×32 or 128×64)** on USB. It tells the player terminal
(`nestris-terminal`) and the capture stations (`nestris-station`) instantly
when a card is placed or taken off, shows the player's name and score on the
display, and writes nicknames onto new cards safely (write, read back,
verify).

- Protocol (JSON lines over USB serial): [docs/PROTOCOL.md](docs/PROTOCOL.md)
- Card layout: [docs/CARD_FORMAT.md](docs/CARD_FORMAT.md)
- Design, wiring and roadmap: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)

> Status: planned (phase R0). The firmware in use until then is the v1 sketch
> in `../RFID_ESP/ESP32_CARD_READER`.

## Build and flash (from phase R1)

```powershell
pip install platformio            # or: uv tool install platformio
pio test -e native                # unit tests of the pure logic on the PC
pio run -e esp32dev -t upload     # build and flash the reader on USB
pio device monitor -b 115200      # watch the JSON lines
```

Releases (tag `v<version>`) publish a ready-to-flash image; the terminal and
NestrisLTM (for stations) update readers from there.
