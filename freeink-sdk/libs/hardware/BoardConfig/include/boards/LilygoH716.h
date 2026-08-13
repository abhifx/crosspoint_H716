#pragma once

// --- LilyGo T5-4.7-S3 H716 (Standard / Touch) — ESP32-S3 ----------------------
// 960x540 16-gray raw parallel panel (ED047TC1). Unlike the Pro version, this
// uses a 74HCT4094D shift register (H716_SR_DATA=13, SR_CLK=12, SR_STR=0) for
// display control and power rails. SDA=18, SCL=17 for touch/RTC.
constexpr BoardProfile LILYGO_T5_H716 = {
    Board::LilyGoT5H716,
    "lilygo_t5_h716",
    InputStyle::DigitalButtons,  // BOOT button on GPIO 0
    DisplayController::LgfxEpd,
    960,
    540,
    {PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED,
     PIN_UNASSIGNED},
    0,
    {11, 16, 15, 42, PIN_UNASSIGNED, false, 40000000},  // SD: SCLK11 MISO16 MOSI15 CS42, 40MHz
    {PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, 21, 21,
     false},
    9,  // batteryAdc: GPIO 9 for H716 Standard
    PIN_UNASSIGNED,
    2.0f,
    43, // usbDetect: GPIO 43 for H716 Standard
    // GT911 touch (SDA18 SCL17 INT10). Portrait sensor on landscape panel.
    {TouchController::Gt911, 18, 17, 10, PIN_UNASSIGNED, 0x5D, 0, 959, 0, 539, false, 0x14, true, true, PIN_UNASSIGNED, true, false, true, true},
    NO_FRONTLIGHT,
    NO_AUDIO,
    NO_LEDS,
    NO_FLIP,
    NO_SDMMC,
    {18, 17, 100000, 0, 0}, // Disable charger check to clean up bus
    NO_MIC,
    {18, 17, 100000, 0x51, 0, 0, 0, RtcType::Pcf8563, ImuType::None}, // RTC (0x51) on 18/17
    1.2f,
    {PIN_UNASSIGNED, PIN_UNASSIGNED}};
