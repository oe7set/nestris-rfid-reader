# CLAUDE.md

Guidance for Claude Code (and other contributors) working in this repository.

## What this is

**nestris-rfid-reader**: PlatformIO/Arduino firmware for the self-built
Retroverse card reader (ESP32 DevKit, RC522 on SPI, SSD1306 on I²C, USB
serial). Hosts: `../nestris-terminal` (Python, `src/nestris_terminal/rfid/`)
and `../nestris-core/crates/nestris-station` (Rust, `src/rfid.rs`). It
replaces the v1 sketch `../RFID_ESP/ESP32_CARD_READER/sketch_rfid_retroverse`
(keep that untouched as reference; its card-presence trick is described in
docs/ARCHITECTURE.md).

Read first: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) (wiring, presence
state machine, display, module layout, phases),
[docs/PROTOCOL.md](docs/PROTOCOL.md) (the host contract),
[docs/CARD_FORMAT.md](docs/CARD_FORMAT.md).

## Commands (from phase R1)

```powershell
pio test -e native                 # unit tests of src/core (no hardware)
pio run -e esp32dev                # build
pio run -e esp32dev -t upload      # flash (reader on USB)
pio device monitor -b 115200
python tools/reader_cli.py COM5    # bench tool: events, commands, card writes
```

## Conventions

- Code comments and docs in **English**; display texts German (default) and
  English.
- **Commit messages never mention Claude** (no Co-Authored-By trailer).
- `docs/PROTOCOL.md` is the contract: change it first, then the firmware and
  both host drivers (terminal and station) in the same change set, and bump
  `proto` for incompatible changes.
- `src/core/` is pure C++ without Arduino or hardware headers and is
  unit-tested with `pio test -e native`; hardware access only in `src/hw/`.
- The main loop never blocks (no `delay` beyond the 50 ms RFID cycle);
  long operations are state machines.
- Serial output is JSON lines only, built in `src/core/protocol`.
- Settings are NVS keys documented in PROTOCOL.md (`config` command); they
  must survive firmware updates.
- Versions come from git tags `v<semver>`; releases are built by GitHub
  Actions and consumed by the apps' updaters (`../nestris-ltm/docs/UPDATES.md`).
