# LilyGo T5-H716 Final Hardware Integration

This plan finalizes the hardware port for the LilyGo T5-H716, enabling Bluetooth (BLE HID) and verifying WiFi connectivity.

## Proposed Changes

### [Component] PlatformIO Build Config
- [MODIFY] [platformio.ini](file:///D:/AI_Development/lilygo/platformio.ini): Add `h2zero/NimBLE-Arduino@^2.3.8` to `lib_deps` for the `lilygo_t5_h716` environment to enable BLE HID host support.

### [Component] Main Application Logic
- [MODIFY] [src/main.cpp](file:///D:/AI_Development/lilygo/src/main.cpp):
    - Initialize the BLE stack using `BleHid.begin()`.
    - Add a one-time BLE scan in `setup()` to verify the Bluetooth radio.
    - Remove temporary bring-up debug logic (manual I2C scanner calls in `setup()`).

### [Component] Peripheral Verification
- [VERIFY] Monitor serial logs for:
    - Successful NimBLE initialization.
    - BLE device discovery.
    - (Optional) WiFi connection if credentials provided.

## Verification Plan

### Automated Tests
- `platformio run -e lilygo_t5_h716 -t upload`: Build and flash to verify no library conflicts and successful start.

### Manual Verification
- Review Serial Monitor for "BLE device found" messages.
- Test side button (GPIO 21) functionality in Reader activity.
- Confirm persistent touch functionality.
