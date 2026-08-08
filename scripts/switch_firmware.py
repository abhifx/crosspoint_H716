import os
import shutil
import sys
from pathlib import Path

def main():
    if len(sys.argv) < 2:
        print("Usage: python switch_firmware.py <firmware_name>")
        print("Example: python switch_firmware.py xteink")
        return

    target_fw = sys.argv[1].lower()
    project_root = Path(__file__).parent.parent
    firmware_root = project_root / "firmware"
    active_src = project_root / "src"

    # Identify available firmwares
    available_fws = [d.name for d in firmware_root.iterdir() if d.is_dir()]

    if target_fw not in available_fws:
        print(f"Error: Firmware '{target_fw}' not found in firmware/ directory.")
        print(f"Available: {', '.join(available_fws)}")
        return

    # Check if 'src' currently holds another firmware
    # We'll look for a .fw_tag file to see what's there
    tag_file = active_src / ".fw_tag"
    current_fw = "unknown"
    if tag_file.exists():
        current_fw = tag_file.read_text().strip()

    if current_fw == target_fw:
        print(f"Firmware '{target_fw}' is already active.")
        return

    print(f"Switching from '{current_fw}' to '{target_fw}'...")

    # 1. Save current src back to its firmware folder (safety)
    if current_fw != "unknown":
        fw_src_dest = firmware_root / current_fw / "src"
        if fw_src_dest.exists():
            shutil.rmtree(fw_src_dest)
        shutil.copytree(active_src, fw_src_dest)
        print(f"Saved current '{current_fw}' to firmware/{current_fw}/src")

    # 2. Clear active src
    if active_src.exists():
        shutil.rmtree(active_src)

    # 3. Copy new firmware to src
    new_fw_src = firmware_root / target_fw / "src"
    shutil.copytree(new_fw_src, active_src)

    # 4. Tag it
    (active_src / ".fw_tag").write_text(target_fw)

    print(f"SUCCESS: Firmware '{target_fw}' is now active in src/.")
    print("You can now build using 'pio run'.")

if __name__ == "__main__":
    main()
