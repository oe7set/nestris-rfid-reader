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

> Status: v1.0.0, tested on a real reader with the terminal (2026-10-04);
> terminal and station speak protocol v2 (phases R1–R3). The
> current terminal and station versions refuse readers that still run the
> v1 sketch (`../RFID_ESP/ESP32_CARD_READER`): flash them first
> ([docs/FLASHING.md](docs/FLASHING.md)).

## Build and flash

```powershell
pip install platformio            # or: uv tool install platformio
pio test -e native                # unit tests of the pure logic on the PC
pio run -e esp32dev -t upload     # build and flash the reader on USB
pio device monitor -b 115200      # watch the JSON lines
```

Releases (tag `v<version>`) publish a factory image (new readers) and an app
image (updates); see [docs/FLASHING.md](docs/FLASHING.md). The terminal and
NestrisLTM (for stations) will update readers from there.

## License

Apache License 2.0, see [LICENSE](LICENSE) and [NOTICE](NOTICE).
Copyright 2026 Erwin Spitaler (OE7SET) – Retroverse.
