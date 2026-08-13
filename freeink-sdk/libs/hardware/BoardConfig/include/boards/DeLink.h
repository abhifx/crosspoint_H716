#pragma once



// --- de-link (X4-class GDEQ0426T82 panel on ESP32-S3) — SSD1677 + frontlight ---
// Reuses the SSD1677 driver (same controller/panel as X4); differs at the board
// level: S3 MCU, SDMMC SD, warm/cool PWM frontlight.
//
// Orientation: this profile ships NO_FLIP (X4 orientation). A board that mounts
// the panel rotated sets `ROTATE_180` (or a mirror) here, and the SSD1677 driver
// applies it in hardware (mirrorX via RAM addressing, mirrorY via gate scan). Any
// board injects its own mount transform the same way.
constexpr BoardProfile DE_LINK = {Board::DeLink,
                                  "de_link",
                                  InputStyle::XteinkAdcLadder,
                                  DisplayController::SSD1677,
                                  800,
                                  480,
                                  {8, 10, 21, 4, 5, 6, PIN_UNASSIGNED},
                                  0,  // displaySpiHz: SSD1677 default (40 MHz)
                                  // SD on de-link is 4-bit SDMMC. SdFat can't drive SDIO, so SDCardManager
                                  // mounts an FsVolume on a native esp-idf SDMMC block device (FREEINK_SD_SDMMC);
                                  // the wiring is in the sdmmc field below. These SPI sd pins are unused.
                                  {39, 38, 40, 41, PIN_UNASSIGNED, true, 0},
                                  {0, 1, 2, 3, 4, 5, 3, true},  // power button active-HIGH (INPUT_PULLDOWN) on de-link
                                  4,  // batteryAdc GPIO4
                                  PIN_UNASSIGNED,
                                  2.0f,
                                  PIN_UNASSIGNED,
                                  NO_TOUCH,
                                  // Primary brightness PWM (GPIO5). Warm/cool/rail/fault pins (GPIO6/7/17/18)
                                  // are not driven.
                                  {5, 20000, 8, true},
                                  NO_AUDIO,
                                  NO_LEDS,
                                  NO_FLIP,
                                  {39, 40, 38, 48, 42, 41, 4},  // SDMMC 4-bit: CLK39 CMD40 D0=38 D1=48 D2=42 D3=41
                                  NO_GAUGE};


