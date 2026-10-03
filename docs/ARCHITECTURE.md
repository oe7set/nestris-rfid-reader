# nestris-rfid-reader: architecture and plan

Firmware for the self-built Retroverse player-card reader: ESP32 + RC522
RFID module + SSD1306 OLED, connected over USB to the player terminal
(`nestris-terminal`) or a capture station (`nestris-station`). It replaces
the v1 sketch in `../RFID_ESP/ESP32_CARD_READER/sketch_rfid_retroverse`.

## Goals

- **Instant, reliable card presence**: an event when a card is placed and
  when it is taken off (≈ 300 ms), no polling on the host side. The v1 sketch
  found the principle (re-detect a lying card every cycle, count misses);
  v2 makes it a tested state machine.
- **Clear protocol** with versions, acknowledgements and a device identity:
  [PROTOCOL.md](PROTOCOL.md).
- **Safe card writes**: write, read back, verify; a marker block tells
  "new card", "Retroverse card" and "damaged write" apart:
  [CARD_FORMAT.md](CARD_FORMAT.md).
- **Readable OLED** with clear states for players (128×32 and 128×64).
- **Robust**: watchdog, RC522 self-check and re-init, works without a host.
- **Updatable from the apps over USB** (no Wi-Fi): GitHub releases with a
  ready-to-flash image, flashed by the terminal and the station.

Decisions (2026-10-03): protocol v2 only, no Wi-Fi, ESP32 DevKit, OLED
128×32 or 128×64, PlatformIO + Arduino, card format v1 + marker block,
updates checked automatically and installed on click.

## Hardware

ESP32 DevKit (ESP32-WROOM-32, `esp32dev`), USB bridge CP2102 or CH340.

| RC522 | ESP32 | | SSD1306 (I²C) | ESP32 |
|---|---|---|---|---|
| SDA (SS) | GPIO 5 | | SDA | GPIO 21 |
| SCK | GPIO 18 | | SCL | GPIO 22 |
| MOSI | GPIO 23 | | VCC | 3.3 V |
| MISO | GPIO 19 | | GND | GND |
| RST | GPIO 4 | | address | `0x3C` (`0x3D` is probed too) |
| IRQ | — | | | |
| 3.3 V / GND | 3.3 V / GND | | | |

The pins are the v1 wiring, so existing readers only need the new firmware.
The display size is a setting (`config display`), not a build variant: one
firmware image for all readers. A missing OLED is detected at boot
(`display: none`); the reader still works.

Optional (planned, not required): the DevKit's on-board LED (GPIO 2) blinks
on card events.

## Card presence

The RC522 only reports *new* cards (`REQA`). To notice a card that stays on
the reader, every cycle (every **50 ms**):

1. `WUPA` (wake-up, also answers cards in HALT state) + anticollision/select
   → UID + SAK, or nothing.
2. If a card answered: `HLTA`, so the next cycle's `WUPA` sees it again.
3. Feed the result into the **presence state machine** (`src/core/presence`,
   pure, unit-tested):

```
             seen(uid)                     not seen for removed_after_ms
   EMPTY ───────────────► PRESENT(uid) ────────────────────────────────► EMPTY
                              │   seen(other uid)                (event: removed)
                              └──► removed(uid) + present(other)
```

   - A new UID is reported only after it was seen in **2 consecutive
     cycles** (debounce against half-read cards at the field's edge).
   - Removed after `removed_after_ms` (default 300 ms = 6 missed cycles).
   - Reading the name (authenticate + 2 blocks, ≈ 25 ms) happens once per
     new card, not every cycle.

If the RC522 stops answering (version register reads `0x00`/`0xFF`, checked
every 5 s and after 20 failed cycles), the firmware resets and re-initialises
it, logs a `warn` and reports `reader: missing` until it is back.

The v1 sketch called `PCD_Init()` after every read to get a similar effect;
`WUPA`/`HLTA` gives the same detection without resetting the antenna each
cycle (less RF dropout, faster).

## Display

Screens (German default, English with `config lang:en`). 128×32 shows two
lines, 128×64 additionally a status bar and up to four lines.

| State | 128×32 | 128×64 extra |
|---|---|---|
| boot | `RETROVERSE` / `Reader 1.0.0` | wiring check: RFID ok, OLED ok |
| idle | `Karte` / `auflegen` (slow card animation) | status bar: PC ✓/✗ |
| card, Retroverse/legacy | **name** / host line (e.g. `Bestwert 159.867`) or `Karte erkannt` | up to 3 host lines |
| card, blank | `Neue Karte` / `am Terminal anmelden` | |
| card, corrupt/unreadable | `Karte defekt` / `neu beschreiben` | |
| card, unsupported | `Karte nicht` / `unterstützt` | |
| writing | `Schreibe …` / `Karte liegen lassen` | progress |
| write ok / failed | `Gespeichert` / name · `Fehler` / reason (3 s) | |
| no host for 10 s | idle screen with `PC getrennt` | |
| RC522 missing | `Leser-Fehler` / `RFID-Modul prüfen` | |

Names use the large font and shrink to fit (15 characters fit at size 1).
**Burn-in protection**: the idle screen moves a few pixels every minute and
dims after 10 minutes without a card; any card restores full brightness.

## Firmware structure (PlatformIO, Arduino core for ESP32 3.x)

```
platformio.ini            envs: esp32dev (firmware), native (unit tests)
src/
  main.cpp                setup/loop: wires the modules, fixed-rate scheduler
  core/                   pure C++ (no Arduino), built for native tests too
    app.{h,cpp}           the behaviour: events, commands, writes, heartbeat,
                          display state; hardware behind IRfid/IDisplay/ILink/ISystem
    uid.h                 UID type (hex, parse)
    settings.h            settings model (NVS keys)
    presence.{h,cpp}      card presence state machine
    card_format.{h,cpp}   block 4/5 encode/decode, CRC-16, format detection
    protocol.{h,cpp}      parse commands / build messages (ArduinoJson)
    screens.{h,cpp}       what to show (texts per state + language), no drawing
  hw/
    rfid_rc522.{h,cpp}    MFRC522 wrapper: WUPA/select/HLTA, read/write sector 1, health
    oled.{h,cpp}          SSD1306 drawing of a screen model, size from settings
    settings_store.{h,cpp} NVS (Preferences) load/save of the config keys
    serial_link.{h,cpp}   line reader (512 B cap) and writer
test/
  test_presence/ test_card_format/ test_protocol/ test_app/   (Unity, env native;
                          test_app runs core::App against fake hardware)
tools/
  reader_cli.py           bench tool: show events, send commands, write a card
  pio_version.py          FW_VERSION/FW_BUILD from git
  pio_merge.py            `-t factory` target (factory image)
  make_release.py         release assets + manifest + SHA256SUMS
  native-zig/             gcc/g++ shims forwarding to zig, for native tests on Windows
docs/                     ARCHITECTURE, PROTOCOL, CARD_FORMAT, FLASHING
```

Libraries (pinned in `platformio.ini`): `miguelbalboa/MFRC522`,
`adafruit/Adafruit SSD1306` + `Adafruit GFX`, `bblanchon/ArduinoJson` 7.

Rules:

- `core/` has no Arduino/hardware includes and is fully unit-tested on the PC
  (`pio test -e native`).
- The loop never blocks longer than one RFID cycle; card writes run as a
  small state machine (wait for card → write → verify), not with delays.
- Serial output is JSON only (built by `core/protocol`), one `print` per line.
- Task watchdog (`esp_task_wdt`, 5 s) on the loop task.

## Versions, releases and flashing

- Version: the git tag `v<semver>`; the build embeds it (`FW_VERSION`) and
  reports it in `hello.fw`.
- A tag builds on GitHub Actions (`.github/workflows/release.yml`,
  assets collected by `tools/make_release.py`):
  - `nestris-rfid-reader-<ver>-esp32dev.bin` – **factory image** (bootloader,
    partitions, app) for offset `0x0`: new readers only, because the merged
    file fills the gap between the partitions with `0xFF` and so **erases the
    NVS settings** at `0x9000`;
  - `nestris-rfid-reader-<ver>-esp32dev-app.bin` – app only for `0x10000`:
    what updates flash (settings stay);
  - `nestris-rfid-reader-<ver>-manifest.json` – version, board, protocol,
    files with offsets and SHA-256;
  - `SHA256SUMS.txt` (+ signature, see the update plan).
- Flashing over USB (no Wi-Fi by decision):
  - **terminal**: hidden menu → *Updates* → *Leser-Firmware aktualisieren*
    (Python `esptool`, closes the reader port meanwhile);
  - **station**: from the NestrisLTM admin page *Stationen* (MQTT command,
    the station flashes with the `espflash` library);
  - **first flash / bench**: `docs/FLASHING.md` (PlatformIO upload,
    `esptool` command line, or the terminal's CLI
    `nestris-terminal flash-reader`).
- Settings live in NVS and survive updates of the app image; flashing the
  factory image resets them. See [FLASHING.md](FLASHING.md).

The common update mechanism of all Retroverse apps (GitHub releases of
`github.com/oe7set/<repo>`, check automatically, install on click) is
described in `../nestris-ltm/docs/UPDATES.md`.

## Host side changes (in the other repositories)

- `nestris-terminal`: new `rfid/protocol.py` + `driver` for v2 (events
  instead of 750 ms polling, `write` with `uid` and acknowledgement, `show`),
  fake driver speaks v2; reader firmware update in the hidden menu.
- `nestris-core` (`nestris-station`): `rfid.rs` for v2; the MQTT `cmd` topic
  accepts `show` (and `write`) instead of `highscore`/`setname`; status
  payload gains `reader_fw` and `reader_serial`; reader flashing.
- `nestris-ltm`: shows the best score on the station's reader via `show`
  when a card is placed (new); device overview with versions.

## Phases

**Status:** R1–R3 are implemented: firmware (35 native tests, builds),
release workflow, and the v2 drivers in the terminal (`c27d928`), the station
(`0870d28`) and NestrisLTM's reader greeting (`c9ee974`), all tested
against simulated readers. **Nothing has run on real hardware yet**; the
bench checks of R1–R3 are the next step once a reader is at hand. Since R3
the terminal and the stations refuse readers with the v1 sketch ("Leser-
Firmware veraltet" / `rfid: outdated`): flash every reader with this
firmware (docs/FLASHING.md, factory image) before using the new versions.

| Phase | Content | Verification |
|---|---|---|
| R0 | Plan, repo, protocol and card format ✅ | review |
| R1 | Firmware core: PlatformIO project, `core/` modules with native tests, RC522 wrapper (WUPA/HLTA presence, sector 1 read/write/verify, health), settings, serial link, `app` | `pio test -e native`; bench with a reader: place/remove, legacy and blank cards, write + verify, pull the RC522 cable, flood the serial line |
| R2 | OLED screens 128×32/128×64, animations, burn-in protection, `tools/reader_cli.py` | photos of every screen on both displays |
| R3 | ✅ Host drivers: terminal v2 driver + fake driver + tests; station `rfid.rs` v2 + MQTT `cmd` mapping + tests; NestrisLTM `show` best score | terminal end to end with a real reader; station replay run with a reader |
| R4 | Release workflow (factory + app image, manifest, checksums) ✅, `FLASHING.md` ✅, flashing from the terminal and the station | flash an old v1 reader to v2 from the terminal; station update via admin |
| R5 | Update functions of all apps (`../nestris-ltm/docs/UPDATES.md`) | update every app from one GitHub release to the next |
