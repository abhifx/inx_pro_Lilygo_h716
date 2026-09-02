"""Build the single-file merged factory image (bootloader + partitions + boot_app0 + application)
used by web flashers (e.g. LilyGo Spark, ESP Web Flasher) and external tools."""

from pathlib import Path
import shutil
import subprocess

Import("env")


def build_factory(source, target, env):
    build = Path(env.subst("$BUILD_DIR"))
    name = env.subst("$PROGNAME")
    app = build / f"{name}.bin"
    bootloader = build / "bootloader.bin"
    partitions = build / "partitions.bin"
    framework = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
    boot_app = framework / "tools" / "partitions" / "boot_app0.bin"

    output_factory = build / f"{name}.factory.bin"
    output_merged = build / f"{name}.merged.bin"

    # Board settings: flash size from board config.
    # Preserve the exact flash_mode (DIO = 0x2) from bootloader.bin via --flash-mode keep
    # so external web flashers (e.g. LilyGo Spark) boot cleanly without flash mode corruption.
    flash_size = env.BoardConfig().get("upload.flash_size", env.BoardConfig().get("build.flash_size", "16MB"))

    files = (app, bootloader, partitions, boot_app)
    if not all(path.is_file() for path in files):
        missing = ", ".join(str(path) for path in files if not path.is_file())
        raise RuntimeError(f"Cannot build merged factory image; missing: {missing}")

    tool = Path(env.subst("$UPLOADER"))
    command = [
        str(tool),
        "--chip",
        "esp32s3",
        "merge-bin",
        "--output",
        str(output_factory),
        "--flash-mode",
        "keep",
        "--flash-freq",
        "keep",
        "--flash-size",
        str(flash_size),
        "0x0",
        str(bootloader),
        "0x8000",
        str(partitions),
        "0xe000",
        str(boot_app),
        "0x10000",
        str(app),
    ]

    print(f"[MERGE] Building single-file all-in-one image: {output_factory}")
    print(f"[MERGE] Config: chip=esp32s3 flash_mode=keep flash_size={flash_size}")
    subprocess.run(command, check=True)

    # Copy as firmware.merged.bin for tools that expect .merged.bin extension
    shutil.copyfile(output_factory, output_merged)
    print(f"[MERGE] Created alias: {output_merged}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", build_factory)
