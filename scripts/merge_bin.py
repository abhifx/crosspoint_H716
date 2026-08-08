import os
import subprocess
from pathlib import Path

Import("env")

def merge_bin_action(source, target, env):
    # This script merges the bootloader, partition table, otadata, and firmware into a single binary
    # that can be flashed at offset 0x0.

    mcu = env.get("BOARD_MCU", "esp32s3")
    build_dir = Path(env.subst("$BUILD_DIR"))
    project_dir = Path(env.subst("$PROJECT_DIR"))

    bootloader = build_dir / "bootloader.bin"
    partitions = build_dir / "partitions.bin"
    firmware = target[0].get_abspath()

    # Locate boot_app0.bin (otadata)
    # This is typically in the framework packages
    packages_dir = Path(os.path.expanduser("~")) / ".platformio" / "packages"
    otadata = packages_dir / "framework-arduinoespressif32" / "tools" / "partitions" / "boot_app0.bin"

    if not otadata.exists():
        print(f"Warning: boot_app0.bin not found at {otadata}. Merged binary might be unstable.")
        otadata = None

    # Use a descriptive name for the merged binary
    version = env.get("CROSSPOINT_VERSION", "v1.5.0")
    if version.startswith('"') and version.endswith('"'):
        version = version[1:-1]

    output_name = f"crosspoint_h716_{version}_merged.bin"
    output = project_dir / output_name

    # Flash params from board config
    flash_mode = env.get("BOARD_BUILD_FLASH_MODE", "qio")
    # T5-H716 uses 16MB Flash
    flash_size = "16MB"

    print(f"\n[Post-Build] Merging binaries for {mcu} (Mode: {flash_mode}, Size: {flash_size})...")

    # Construct the command
    cmd = [
        "python", "-m", "esptool",
        "--chip", mcu,
        "merge_bin",
        "-o", str(output),
        "--flash_mode", flash_mode,
        "--flash_size", flash_size,
        "0x0000", str(bootloader),
        "0x8000", str(partitions),
        "0x10000", str(firmware)
    ]

    # Insert otadata if found (at 0xe000 per partitions.csv)
    if otadata:
        cmd.extend(["0xe000", str(otadata)])

    try:
        # Run the command and capture output
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            print(f"SUCCESS: Merged binary created at {output}")
            # Also update the generic copy
            generic_output = project_dir / "firmware_merged.bin"
            if os.path.exists(generic_output):
                os.remove(generic_output)
            import shutil
            shutil.copy(output, generic_output)
        else:
            print(f"FAILED to merge binaries:\n{result.stderr}")
    except Exception as e:
        print(f"ERROR running merge_bin: {e}")

# Register the post-action to run after the firmware.bin is generated
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin_action)
