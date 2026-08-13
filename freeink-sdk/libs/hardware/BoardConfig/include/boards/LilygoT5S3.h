#pragma once



// LilyGo T5 S3 Pro Lite GT911 touch (shared I2C bus). The digitizer reports a
// portrait 540x960 frame on the landscape 960x540 panel, so swap axes into the
// panel-native display frame before app-level orientation mapping.
constexpr TouchConfig LILYGO_T5_PRO_GT911 = {
    TouchController::Gt911, 39, 40, 3, 9, 0x5D, 0, 959, 0, 539, false, 0x14, false, true, PIN_UNASSIGNED, true,
    false, true};  // powerEnable, swapXY=true, flipX=false, flipY=true

// --- LilyGo T5 S3 4.7" (ED047TC1 raw-parallel EPD) — ESP32-S3 -----------------
// 960x540 16-gray raw parallel panel driven via LovyanGFX (FREEINK_DRIVER_LGFX_EPD);
// the panel can't power up without the board's PMIC (TPS65185) + PCA9535 expander
// sequence, which the board injects through LgfxEpdConfig::power (see the LilyGo
// support doc). Geometry is the physical/native landscape scan size; app-level
// orientation handles rotated reader layouts. Display + GT911 touch + PWM backlight + the I2C fuel gauge
// (BQ27220/BQ25896) are wired here. The user button (behind the PCA9535 expander),
// PCF85063 RTC, and LoRa/GPS remain board-support — see docs/lilygo-t5s3-support.md.
constexpr BoardProfile LILYGO_T5S3 = {
    Board::LilyGoT5S3,
    "lilygo_t5s3",
    InputStyle::DigitalButtons,  // only BOOT (GPIO0) is a direct GPIO; the user
                                 // button is behind the PCA9535 expander (board-support)
    DisplayController::LgfxEpd,
    960,
    540,
    {PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED,
     PIN_UNASSIGNED},                            // no SPI display pins: parallel bus lives in LgfxEpdConfig
    0,                                           // displaySpiHz n/a (external bus)
    {14, 21, 13, 12, PIN_UNASSIGNED, false, 0},  // SD over SPI: SCLK14 MISO21 MOSI13 CS12
    {PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, 0,
     false},         // power=BOOT (GPIO0), active-low
    PIN_UNASSIGNED,  // batteryAdc: none — uses the I2C fuel gauge below
    PIN_UNASSIGNED,
    2.0f,
    PIN_UNASSIGNED,
    LILYGO_T5_PRO_GT911,  // GT911 touch (SDA39 SCL40 INT3 RST9, 0x5D, portrait sensor -> landscape panel)
    {11, 5000, 8, true},  // backlight: BL_EN GPIO11, PWM 5 kHz / 8-bit, active-high
    NO_AUDIO,
    NO_LEDS,
    NO_FLIP,
    NO_SDMMC,
    {39, 40, 400000, 0x55, 0x6B},  // BQ27220 gauge (0x55) + BQ25896 charger (0x6B) on SDA39/SCL40
    NO_MIC,
    NO_SENSORS,
    1.2f,  // uiScale: 4.7" 960x540 touch (~234 PPI) — finger-sized chrome, like Sticky
    // Power latch: main-power MOSFET on GPIO2, driven HIGH first thing in boot
    // via holdPowerRails() or the board powers off when USB is unplugged.
    {2}};


