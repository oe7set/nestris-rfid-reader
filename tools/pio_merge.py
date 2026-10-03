"""PlatformIO post-script: `pio run -e esp32dev -t factory` writes
.pio/build/esp32dev/factory.bin = bootloader + partitions + boot_app0 + app,
to be flashed at 0x0 on a NEW reader.

Do not use it for updates: the image fills the gap between the partitions
with 0xFF, which erases the NVS settings at 0x9000. Updates flash only the app
(firmware.bin) at 0x10000.
"""

import os

Import("env")  # noqa: F821

OFFSETS = (("0x1000", "bootloader.bin"), ("0x8000", "partitions.bin"))


def factory(source, target, env):
    build = env.subst("$BUILD_DIR")
    platform = env.PioPlatform()
    boot_app0 = os.path.join(
        platform.get_package_dir("framework-arduinoespressif32"), "tools", "partitions", "boot_app0.bin"
    )
    esptool = os.path.join(platform.get_package_dir("tool-esptoolpy"), "esptool.py")
    parts = " ".join(f'{off} "{os.path.join(build, name)}"' for off, name in OFFSETS)
    out = os.path.join(build, "factory.bin")
    cmd = (
        f'"$PYTHONEXE" "{esptool}" --chip esp32 merge_bin -o "{out}" '
        f"--flash_mode keep --flash_freq keep --flash_size keep "
        f'{parts} 0xe000 "{boot_app0}" 0x10000 "{os.path.join(build, "firmware.bin")}"'
    )
    if env.Execute(cmd):
        env.Exit(1)
    print(f"factory image: {out}")


env.AddCustomTarget(  # noqa: F821
    "factory",
    "$BUILD_DIR/${PROGNAME}.bin",
    factory,
    title="Factory image",
    description="bootloader + partitions + app in one file for 0x0 (new readers only)",
)
