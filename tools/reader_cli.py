# /// script
# requires-python = ">=3.11"
# dependencies = ["pyserial>=3.5"]
# ///
"""Bench tool for the reader: shows every line it sends and sends commands.

    uv run tools/reader_cli.py COM5            # Windows
    uv run tools/reader_cli.py /dev/ttyUSB0    # Linux

Commands (type and press Enter):
    h                  hello
    w NAME [UID]       write NAME onto the card (optionally only card UID)
    s LINE1|LINE2      show lines while the card lies on the reader
    c key=value ...    config, e.g. "c display=128x64 lang=en"
    x                  cancel a pending write
    r                  reboot
    {...}              any raw JSON line
    q                  quit
A ping is sent every 3 s, like the real hosts do.
"""

from __future__ import annotations

import itertools
import json
import sys
import threading
import time

import serial

ids = itertools.count(1)


def send(port: serial.Serial, msg: dict) -> None:
    msg.setdefault("id", next(ids))
    line = json.dumps(msg, separators=(",", ":"))
    port.write((line + "\n").encode())
    print(f"  >> {line}")


def reader(port: serial.Serial, stop: threading.Event) -> None:
    while not stop.is_set():
        raw = port.readline()
        if not raw:
            continue
        text = raw.decode(errors="replace").rstrip()
        stamp = time.strftime("%H:%M:%S")
        try:
            msg = json.loads(text)
        except ValueError:
            print(f"{stamp} (text) {text}")
            continue
        if msg.get("type") == "status" and "-v" not in sys.argv:
            continue  # heartbeat; show with -v
        print(f"{stamp} << {text}")


def pinger(port: serial.Serial, stop: threading.Event) -> None:
    while not stop.wait(3):
        port.write(b'{"type":"ping"}\n')


def parse_value(v: str) -> object:
    if v in ("true", "false"):
        return v == "true"
    try:
        return int(v)
    except ValueError:
        return v


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    port = serial.Serial(sys.argv[1], 115200, timeout=0.2)
    stop = threading.Event()
    threading.Thread(target=reader, args=(port, stop), daemon=True).start()
    threading.Thread(target=pinger, args=(port, stop), daemon=True).start()
    print(f"connected to {sys.argv[1]} - 'q' quits")
    try:
        for line in sys.stdin:
            cmd = line.strip()
            if not cmd:
                continue
            if cmd == "q":
                break
            if cmd.startswith("{"):
                port.write((cmd + "\n").encode())
            elif cmd == "h":
                send(port, {"type": "hello"})
            elif cmd == "x":
                send(port, {"type": "cancel"})
            elif cmd == "r":
                send(port, {"type": "reboot"})
            elif cmd.startswith("w "):
                parts = cmd[2:].strip().rsplit(" ", 1)
                msg: dict = {"type": "write", "name": cmd[2:].strip()}
                if len(parts) == 2 and len(parts[1]) in (8, 14, 20):
                    msg = {"type": "write", "name": parts[0], "uid": parts[1]}
                send(port, msg)
            elif cmd.startswith("s "):
                send(port, {"type": "show", "lines": cmd[2:].split("|")})
            elif cmd.startswith("c "):
                pairs = (p.split("=", 1) for p in cmd[2:].split())
                send(port, {"type": "config", **{k: parse_value(v) for k, v in pairs}})
            else:
                print("unknown command (see the help at the top of this file)")
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        port.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
