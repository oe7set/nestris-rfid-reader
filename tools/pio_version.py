"""PlatformIO pre-script: FW_VERSION / FW_BUILD from git.

FW_VERSION is the tag without the "v" when HEAD is tagged (v1.2.0 -> 1.2.0),
otherwise "<last tag>-dev+<commit>" (or "0.0.0-dev+<commit>" without tags).
"""

import datetime
import subprocess

Import("env")  # noqa: F821  (PlatformIO/SCons builtin)


def git(*args):
    try:
        return subprocess.check_output(["git", *args], stderr=subprocess.DEVNULL, text=True).strip()
    except Exception:
        return ""


exact = git("describe", "--tags", "--exact-match", "--match", "v*")
if exact:
    version = exact[1:]
else:
    last = git("describe", "--tags", "--abbrev=0", "--match", "v*")
    commit = git("rev-parse", "--short", "HEAD") or "nogit"
    version = f"{last[1:] if last else '0.0.0'}-dev+{commit}"
build = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

env.Append(CPPDEFINES=[("FW_VERSION", env.StringifyMacro(version)), ("FW_BUILD", env.StringifyMacro(build))])  # noqa: F821
print(f"nestris-rfid-reader {version} ({build})")
