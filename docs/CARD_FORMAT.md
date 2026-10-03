# Card format

Player cards are **MIFARE Classic 1K** (4K works the same). The firmware uses
sector 1 with the factory default key A (`FF FF FF FF FF FF`); the sector
trailer (block 7) is never written, so cards stay rewritable by every reader.

The card only carries the nickname for display and for old tools. **The
player's identity is the card's UID**, linked to the player in NestrisLTM
(`player_cards`); a card renamed by a phone app does not move games to another
player.

## Blocks

| Block | Content |
|---|---|
| 4 | name: ASCII, 1–15 bytes, padded with `0x00` (the v1 layout, unchanged) |
| 5 | v2 marker (below) |
| 6 | reserved, written as zeros |
| 7 | sector trailer, untouched |

Block 5:

| Bytes | Content |
|---|---|
| 0–3 | magic `52 56 52 53` (`"RVRS"`) |
| 4 | format version, `0x01` |
| 5 | name length (1–15) |
| 6–7 | CRC-16/CCITT-FALSE (poly `0x1021`, init `0xFFFF`) over all 16 bytes of block 4, big endian |
| 8–15 | reserved, zeros (readers ignore them) |

## Reading → `format`

1. Not MIFARE Classic (SAK) → `unsupported`.
2. Authentication of sector 1 or reading block 4/5 fails → `unreadable`.
3. Block 5 starts with `RVRS`:
   - version 1, length 1–15, CRC matches, the name is printable ASCII
     followed only by zeros → `retroverse`;
   - anything else → `corrupt`.
4. No marker: block 4 holds printable ASCII (after trimming spaces and
   zeros, 1–16 characters) → `legacy` (written by the v1 firmware or old
   tools; v1 allowed 16 characters, the name is reported as stored).
5. Otherwise (all `0x00`, all `0xFF`, garbage) → `blank`.

## Writing

`write` writes block 4 (name + zero padding), then block 5 (marker with the
CRC of the new block 4), then reads both back and compares. An interrupted
write leaves a CRC mismatch (`corrupt`) or the old content, never a valid
card with a half-written name. Legacy cards become `retroverse` cards the
first time they are written.

## Test vectors

| Name | Block 4 (hex) | CRC |
|---|---|---|
| `Erv` | `45 72 76 00 … 00` | see `test/test_card_format` |

The unit tests in `test/test_card_format/` are the reference implementation's
vectors; the Python (`nestris-terminal`) and Rust (`nestris-station`) sides do
not decode blocks themselves, they only use the `format` reported by the
reader.
