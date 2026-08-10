import os
import subprocess
from pathlib import Path
import shutil

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
    home = Path(os.path.expanduser("~"))
    packages_dir = home / ".platformio" / "packages"
    otadata = packages_dir / "framework-arduinoespressif32" / "tools" / "partitions" / "boot_app0.bin"

    if not otadata.exists():
        # Try finding it relative to the environment's platform packages if possible
        # This is a fallback
        print(f"Warning: boot_app0.bin not found at {otadata}. Trying fallback...")
        # Search in platformio home
        for p in packages_dir.glob("framework-arduinoespressif32*/tools/partitions/boot_app0.bin"):
            otadata = p
            break

    # Locate esptool.py
    esptool_py = packages_dir / "tool-esptoolpy" / "esptool.py"
    if not esptool_py.exists():
        esptool_py = "esptool.py" # Fallback to path

    # Use a descriptive name for the merged binary
    version = env.get("CROSSPOINT_VERSION", "v1.5.0")
    if version.startswith('"') and version.endswith('"'):
        version = version[1:-1]

    output_name = f"crosspoint_h716_{version}_merged.bin"
    output = project_dir / output_name

    # Flash params from board config
    flash_mode = env.get("BOARD_BUILD_FLASH_MODE", "qio")
    # T5-H716 uses 16MB Flash, 80MHz
    flash_freq = "80m"
    flash_size = "16MB"

    print(f"\n[Post-Build] Merging binaries for {mcu} (Mode: {flash_mode}, Freq: {flash_freq}, Size: {flash_size})...")

    # Construct the command
    # We use the same python that is running this script
    import sys
    cmd = [
        sys.executable, str(esptool_py),
        "--chip", mcu,
        "merge_bin",
        "-o", str(output),
        "--flash_mode", flash_mode,
        "--flash_freq", flash_freq,
        "--flash_size", flash_size,
        "0x0000", str(bootloader),
        "0x8000", str(partitions),
        "0x10000", str(firmware)
    ]

    if otadata.exists():
        print(f"Adding otadata from: {otadata}")
        cmd.extend(["0xe000", str(otadata)])
    else:
        print("CRITICAL WARNING: boot_app0.bin NOT FOUND! The merged binary will likely FAIL TO BOOT on OTA partition schemes.")

    try:
        # Run the command and capture output
        print(f"Running: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            print(f"SUCCESS: Merged binary created at {output}")
            # Also update the generic copy
            generic_output = project_dir / "firmware_merged.bin"
            if os.path.exists(generic_output):
                os.remove(generic_output)
            shutil.copy(output, generic_output)
            print(f"Copied to {generic_output}")
        else:
            print(f"FAILED to merge binaries:\n{result.stderr}\n{result.stdout}")
    except Exception as e:
        print(f"ERROR running merge_bin: {e}")

# Register the post-action to run after the firmware.bin is generated
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin_action)
