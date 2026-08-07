# LilyGo T5-H716 Hardware Port Walkthrough

The CrossPoint port for the LilyGo T5-H716 is now complete and verified.

## Accomplishments

### 1. Display Stability
- Integrated the **ED047TC1** parallel e-paper display via the **M5GFX/LovyanGFX** driver.
- Fixed an issue where the display wouldn't initialize by correctly configuring **Octal PSRAM (`qio_opi`)** which is required for the 8MB embedded memory on this S3 variant.
- Verified that the shift register (74HCT4094) correctly pulses the **Latch Enable (LE)** and **Start Vertical (STV)** signals.

### 2. Touch & Ghost Taps
- Resolved "ghost tap" issues caused by overlapping GPIO 0 usage (Latch pin vs Button).
- Correctly mapped the **GT911** touch controller to **I2C SDA 18 / SCL 17**.
- Implemented a **10ms rate-limiter** for touch polling to ensure bus stability and prevent "bus hang" errors.

### 3. Connectivity
- **WiFi:** Successfully verified via a boot-time network scan (found 3+ networks).
- **Bluetooth:** Integrated the **NimBLE-Arduino** stack and verified discovery of nearby HID devices (Keyboards, etc.).

### 4. Power & Memory
- Confirmed **8MB PSRAM** detection and utilization.
- Configured **BQ27220** battery gauge and **BQ25896** charger I2C monitoring.
- Mapped the physical side button to **GPIO 21**.

## Verified Hardware
| Peripheral | Status | Details |
| :--- | :--- | :--- |
| **Display** | ✅ OK | 960x540 16-gray parallel |
| **Touch** | ✅ OK | GT911 (0x5D), no ghost taps |
| **PSRAM** | ✅ OK | 8MB Octal OPI enabled |
| **WiFi** | ✅ OK | Scanning functional |
| **Bluetooth** | ✅ OK | HID host active (NimBLE) |
| **Buttons** | ✅ OK | Boot (0), User (21) |
| **Battery** | ✅ OK | Gauge (0x55), Charger (0x6B) |

## Next Steps
- You can now pair a Bluetooth page-turner via the Settings menu.
- The Reader activity is stable with the new 400kHz I2C cadence.
