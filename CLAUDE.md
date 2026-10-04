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

## Commands

```powershell
pio test -e native                 # unit tests of src/core (no hardware; needs gcc)
pio run -e esp32dev                # build
pio run -e esp32dev -t upload      # flash the app (reader on USB, keeps settings)
pio run -e esp32dev -t factory     # factory image for new readers (FLASHING.md)
pio device monitor -b 115200
uv run tools/reader_cli.py COM5    # bench tool: events, commands, card writes
python tools/make_release.py 1.0.0 # release assets into dist/ (CI does this on tags)
```

`pio` is `~/.platformio/penv/Scripts/pio.exe` if it is not on PATH. Windows
without gcc: put the zig shims first on PATH for the native tests:
`$env:PATH = "$PWD\tools\native-zig;$env:PATH"; pio test -e native`
(zig comes from PyPI via `uvx`, nothing to install).

Status: v1.0.0 runs on a real reader (bench + terminal registration tested
2026-10-04); open hardware checks are listed in docs/ARCHITECTURE.md.

## Conventions

- Code comments and docs in **English**; display texts German (default) and
  English.
- **Commit messages never mention Claude** (no Co-Authored-By trailer).
- License: Apache-2.0 (`LICENSE`), attribution and third-party material in `NOTICE`.
  New third-party assets (fonts, icons, copied code) get an entry there and keep
  their own license file next to them; LICENSE and NOTICE ship with every build.
- `docs/PROTOCOL.md` is the contract: change it first, then the firmware and
  both host drivers (terminal and station) in the same change set, and bump
  `proto` for incompatible changes.
- `src/core/` is pure C++ without Arduino or hardware headers and is
  unit-tested with `pio test -e native`; hardware access only in `src/hw/`.
- Behaviour lives in `src/core/app.cpp` behind the interfaces `IRfid`,
  `IDisplay`, `ILink`, `ISystem`; `test/test_app` drives it with fakes. New
  behaviour gets a test there first.
- Updates flash the **app image at 0x10000**; the factory image (0x0) erases
  the NVS settings and is for new readers only.
- The main loop never blocks (no `delay` beyond the 50 ms RFID cycle);
  long operations are state machines.
- Serial output is JSON lines only, built in `src/core/protocol`.
- Settings are NVS keys documented in PROTOCOL.md (`config` command); they
  must survive firmware updates.
- Versions come from git tags `v<semver>`; releases are built by GitHub
  Actions and consumed by the apps' updaters (`../nestris-ltm/docs/UPDATES.md`).
