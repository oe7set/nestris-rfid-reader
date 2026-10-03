"""Collect the release assets into dist/ (run after `pio run -e esp32dev -t factory`).

    python tools/make_release.py 1.2.0

dist/
  nestris-rfid-reader-<v>-esp32dev.bin          factory image, flash at 0x0 (new readers; erases settings)
  nestris-rfid-reader-<v>-esp32dev-app.bin      app only, flash at 0x10000 (updates; keeps settings)
  nestris-rfid-reader-<v>-manifest.json         what the updaters read
  SHA256SUMS.txt
"""

from __future__ import annotations

import hashlib
import json
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / ".pio" / "build" / "esp32dev"
DIST = ROOT / "dist"
BOARD = "esp32dev"
PROTOCOL = 2


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    version = sys.argv[1].removeprefix("v")
    DIST.mkdir(exist_ok=True)
    files = [
        ("factory", BUILD / "factory.bin", f"nestris-rfid-reader-{version}-{BOARD}.bin", "0x0"),
        ("app", BUILD / "firmware.bin", f"nestris-rfid-reader-{version}-{BOARD}-app.bin", "0x10000"),
    ]
    entries = []
    for kind, src, name, offset in files:
        if not src.is_file():
            print(f"missing {src}: run 'pio run -e {BOARD} -t factory' first", file=sys.stderr)
            return 1
        dst = DIST / name
        shutil.copyfile(src, dst)
        entries.append({"kind": kind, "name": name, "offset": offset, "size": dst.stat().st_size,
                        "sha256": sha256(dst)})
    manifest = {
        "name": "nestris-rfid-reader",
        "version": version,
        "board": BOARD,
        "chip": "esp32",
        "proto": PROTOCOL,
        "files": entries,
        "notes": "Updates flash the 'app' file at its offset (keeps the reader's settings); "
                 "'factory' is for new readers and erases the settings.",
    }
    manifest_path = DIST / f"nestris-rfid-reader-{version}-manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    sums = [f"{sha256(DIST / e['name'])}  {e['name']}" for e in entries]
    sums.append(f"{sha256(manifest_path)}  {manifest_path.name}")
    (DIST / "SHA256SUMS.txt").write_text("\n".join(sums) + "\n", encoding="ascii")
    print("\n".join(sums))
    return 0


if __name__ == "__main__":
    sys.exit(main())
