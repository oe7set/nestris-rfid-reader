# Reader protocol v2

The contract between the reader firmware and its hosts: the player terminal
(`nestris-terminal`, Python, `rfid/`) and the capture station
(`nestris-core/crates/nestris-station`, Rust, `rfid.rs`). **This document is
the source of truth**; change it first, then the firmware and both drivers.

v2 replaces the v1 firmware (`RFID_ESP/ESP32_CARD_READER`), which printed a
`login` line every 750 ms. v2 is event based, acknowledges every command and
says who it is. There is no v1 compatibility mode.

## Transport

- USB serial (CP2102 / CH340 on the ESP32 DevKit), **115200 baud, 8N1**.
- One JSON object per line, UTF-8, terminated by `\n` (the firmware accepts
  `\r\n`). Maximum line length **512 bytes** in both directions; longer lines
  are discarded (the firmware answers `error: line_too_long`).
- Lines that are not JSON objects must be ignored by hosts: the ESP32 boot ROM
  prints text after a reset. After the boot the firmware prints JSON only.
- Opening the port toggles DTR/RTS on most USB bridges and **resets the
  ESP32**. Hosts must expect a `hello` within ~2 s after opening.

## Conventions

- Every message has `"type"`.
- UIDs: uppercase hex without separators, 4, 7 or 10 bytes (`"04A1B2C3"`).
- Names: printable ASCII (0x20–0x7E), 1–15 characters (one MIFARE block
  minus a terminating zero), no leading/trailing spaces.
- Commands may carry `"id"` (integer 0–2^31); the answer echoes it.
  Hosts should always set one.
- Unknown fields are ignored (forward compatible). Unknown command types are
  answered with `result` `ok:false, error:"unknown_type"`.

## Reader → host

### `hello`

On boot and as the answer to the `hello` command.

```json
{"type":"hello","proto":2,"fw":"1.0.0","build":"2026-10-04T12:00:00Z","board":"esp32dev",
 "serial":"A4CF12B3C4D5","display":"128x32","lang":"de","reader":"ok","chip":"0x92",
 "card":null,"id":3}
```

| Field | Meaning |
|---|---|
| `proto` | protocol version; hosts refuse to work with anything but `2` and say so |
| `fw`, `build` | firmware version (semver) and build time; used by the updaters |
| `board` | build target, selects the firmware file for updates (`esp32dev`) |
| `serial` | the ESP32's factory MAC, identifies the physical reader |
| `display` | `128x32`, `128x64` or `none` (no OLED found) |
| `reader` | `ok` or `missing` (RC522 not answering) |
| `chip` | RC522 version register (`0x91`/`0x92` = genuine, `0x88`/`0xB2` = clones) |
| `card` | the card on the reader right now (same object as in `card`), or `null` |
| `id` | only when answering a `hello` command |

### `card`

Sent once per change, immediately (no polling interval on the host side).

```json
{"type":"card","state":"present","uid":"04A1B2C3","name":"Erv","format":"retroverse"}
{"type":"card","state":"removed","uid":"04A1B2C3"}
```

`format` describes what is stored on the card (see [CARD_FORMAT.md](CARD_FORMAT.md)):

| `format` | `name` | Meaning |
|---|---|---|
| `retroverse` | the name | written by v2 firmware, checksum OK |
| `legacy` | the name | name in block 4 without the v2 marker (cards written by v1) |
| `blank` | `null` | MIFARE Classic without a name: a new card |
| `corrupt` | `null` | v2 marker present but checksum wrong (interrupted write) |
| `unreadable` | `null` | authentication/read failed (other keys, card moved away) |
| `unsupported` | `null` | not a MIFARE Classic 1K/4K (e.g. Ultralight, phone) |

A card counts as removed after it was not seen for `removed_after_ms`
(default 300 ms, see [ARCHITECTURE.md](ARCHITECTURE.md#card-presence)). Taking a card off and putting it
back within that time produces no events.

### `status`

Heartbeat every **2 s**; lets hosts detect a hung reader and resynchronize
after a reconnect without waiting for the next `card` event.

```json
{"type":"status","card":{"uid":"04A1B2C3","name":"Erv","format":"retroverse"},"reader":"ok","uptime_s":3711,"host":true}
```

`host` is `true` while the firmware hears from a host (any command within the
last 10 s); the OLED shows "PC getrennt" otherwise.

### `result`

The answer to every command that is not answered by `hello`.

```json
{"type":"result","id":7,"ok":true}
{"type":"result","id":7,"ok":true,"uid":"04A1B2C3","name":"Erv"}
{"type":"result","id":8,"ok":false,"error":"timeout","detail":"no card within 15000 ms"}
```

| `error` | Meaning |
|---|---|
| `bad_request` | missing/invalid field (`detail` names it) |
| `unknown_type` | command not known to this firmware |
| `busy` | another write is pending |
| `timeout` | no (matching) card within `timeout_ms` |
| `wrong_card` | a card is present but its UID is not the requested `uid` |
| `no_card` | `show` without a card on the reader |
| `unsupported_card` | not a MIFARE Classic card |
| `auth` | authentication with the default key failed |
| `write` | the write command failed (card moved?) |
| `verify` | read-back after the write did not match |
| `cancelled` | cancelled by `cancel` |
| `line_too_long`, `bad_json` | the command line could not be parsed (`id` is `null`) |

### `log`

Diagnostics for the hosts' logs; never needed for function.

```json
{"type":"log","level":"warn","msg":"RC522 not answering, re-initialising"}
```

Levels: `debug` (only with `config debug:true`), `info`, `warn`, `error`.

## Host → reader

### `hello`

`{"type":"hello","id":1}` → `hello` with `"id":1`. Hosts send it after
opening the port if no `hello` arrived within 2 s (port opened without reset).

### `ping`

`{"type":"ping","id":2}` → `{"type":"result","id":2,"ok":true}`. Hosts send a
`ping` (or any other command) at least every **5 s**: it keeps `host:true`
and the display out of "PC getrennt".

### `write`

Write a name onto a card (block 4 + marker block 5) and verify it by reading
it back.

```json
{"type":"write","id":7,"name":"Erv","uid":"04A1B2C3","timeout_ms":15000}
```

| Field | |
|---|---|
| `name` | required, see conventions |
| `uid` | optional: only write to this card; another card → keep waiting, answer `wrong_card` only at the timeout |
| `timeout_ms` | optional, 1000–60000, default 15000: how long to wait for a (matching) card |

If the card is already on the reader, the write starts immediately.
The display shows "Schreibe … Karte liegen lassen" meanwhile. On success the
reader also sends a fresh `card` `present` event with the new name (same
`uid`), so hosts that only follow `card` events stay correct.

### `show`

Text for the display while the current card lies on the reader, e.g. the
player's best score. Cleared when the card is removed.

```json
{"type":"show","id":9,"uid":"04A1B2C3","lines":["Erv","Bestwert 159.867"]}
```

| Field | |
|---|---|
| `lines` | 1–4 strings (128×32 shows the first 2), each ≤ 21 characters; longer lines are cut |
| `uid` | optional: only apply if this card is (still) present, else `result` `ok:false, error:"wrong_card"`; without any card: `no_card` |
| `ttl_ms` | optional: revert to the default card screen after this time |

### `cancel`

`{"type":"cancel","id":10}` cancels a pending `write` (that write answers
`cancelled`); answered with `ok:true` also when nothing was pending.

### `config`

Persistent settings (stored in NVS, survive power loss and firmware updates).
Only the given keys change; the answer is a `result` followed by a fresh `hello`.

```json
{"type":"config","id":11,"display":"128x64","lang":"en","removed_after_ms":300,"brightness":200,"flip":false,"debug":false}
```

| Key | Values | Default |
|---|---|---|
| `display` | `128x32`, `128x64`, `none` | `128x32` |
| `lang` | `de`, `en` (display texts) | `de` |
| `removed_after_ms` | 150–2000 | 300 |
| `brightness` | 0–255 (OLED contrast) | 200 |
| `flip` | rotate the display by 180° | `false` |
| `debug` | send `log` `debug` lines | `false` |

### `reboot`

`{"type":"reboot","id":12}` → `result` `ok:true`, then the reader restarts
and sends `hello`.

## Host behaviour (both drivers)

1. Open the port, wait for `hello` (else send `hello`). Refuse `proto != 2`
   with a clear message ("Leser-Firmware zu alt/neu: bitte aktualisieren").
2. Track the card from `card` events; take `hello.card` / `status.card` as
   the truth after (re)connecting.
3. Send `ping` every 3 s. Consider the reader lost after 6 s without any
   line (two missed `status` heartbeats); close and reopen the port.
4. Correlate `result` by `id`; a write's host timeout is `timeout_ms + 3 s`.
5. Log `log` lines at their level.

## Changes

| Version | |
|---|---|
| 2 | first event-based protocol (this document) |
