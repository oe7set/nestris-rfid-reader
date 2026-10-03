# Flashing a reader

Two images per release (see `tools/make_release.py`):

| File | Offset | Use |
|---|---|---|
| `nestris-rfid-reader-<v>-esp32dev.bin` | `0x0` | **new reader** or a reader with the v1 sketch: bootloader + partitions + app. Resets the reader's settings (display size, language …). |
| `nestris-rfid-reader-<v>-esp32dev-app.bin` | `0x10000` | **update** of a v2 reader: app only, keeps the settings. This is what the terminal and the stations flash. |

The reader's USB port must not be open in another program (terminal,
station, serial monitor) while flashing.

## From source (PlatformIO)

```powershell
pio run -e esp32dev -t upload          # build + flash the app (keeps settings)
pio run -e esp32dev -t factory         # build the factory image (.pio/build/esp32dev/factory.bin)
pio device monitor -b 115200           # watch the JSON lines; expect {"type":"hello",...}
```

## From a release (esptool)

```powershell
pip install esptool
# new reader / v1 reader:
esptool --chip esp32 --port COM5 --baud 921600 write_flash 0x0 nestris-rfid-reader-1.0.0-esp32dev.bin
# update:
esptool --chip esp32 --port COM5 --baud 921600 write_flash 0x10000 nestris-rfid-reader-1.1.0-esp32dev-app.bin
```

If the board does not enter the bootloader by itself, hold **BOOT** while
the flashing starts. Check the checksums against `SHA256SUMS.txt` first.

## After flashing

1. `uv run tools/reader_cli.py COM5` shows `hello` with the new `fw`.
2. Set the display if it is a 128×64: `c display=128x64` (stored in the reader).
3. Place a card: a `card present` line appears and the name is on the OLED.
