#pragma once



// Sticky has no output codec (PDM mic in only) — just the LEDC buzzer on GPIO48,
// driven by the Buzzer lib. output=None so hasAudio() stays false; the buzzer
// field carries the tone pin (mirrors how MURPHY_AUDIO carries its buzzer).
constexpr AudioConfig STICKY_AUDIO = {AudioOutput::None,    PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED,
                                      PIN_UNASSIGNED,       PIN_UNASSIGNED, true,           PIN_UNASSIGNED,
                                      PIN_UNASSIGNED,       PIN_UNASSIGNED, 0,              48};

// --- Sticky (Seeed Sticky) — ESP32-S3R8, SSD1677 + GT911 touch ---------------
// 3.97" 800x480 B/W e-paper on a 24-pin FPC, controller confirmed SSD1677 by the
// vendor peripheral demo (pin_config.h: "E-paper SSD1677 (SPI)") — same driver,
// controller, and resolution as X4/de-link; its GDR/RESE/BS1 + dual VSH1/VSH2 +
// external VGH/VGL/VSL/VCOM charge pump is the SSD1677 reference circuit.
// Capacitive GT911 touch on its own I2C bus, MicroSD over SPI (shared display
// bus), BQ27220 fuel gauge, PDM mic + buzzer. Pins are triple-sourced (V01
// schematic 2026-06-05 + porting spec + vendor demo pin_config.h).
//
// Pending hardware validation:
//   * orientation — panel mount transform unknown; ships NO_FLIP (set ROTATE_180/
//     a mirror here once the reader's "up" is confirmed on a unit).
//   * MicroSD shares the display SPI bus; the vendor demo doesn't exercise SD, so
//     bus-sharing / CS arbitration with the panel needs a hardware check.
//   * PDM mic pins (19/20/38) are from the schematic/spec; no vendor demo uses the
//     mic, so they're unconfirmed in code.
constexpr BoardProfile STICKY = {
    Board::Sticky,
    "sticky",
    InputStyle::DigitalConfirmPowerHold,  // shared OK/PWR: click confirms, hold sleeps
    DisplayController::SSD1677,
    800,
    480,
    {13, 14, 15, 16, 17, 18, 47},  // SCK13 MOSI(SDI)14 CS15 DC16 RST17 BUSY18, EP_PWR_EN47
    0,  // displaySpiHz: 0 -> SSD1677 driver default (40 MHz), as on X4/de-link (same controller). The
        // vendor peripheral demo clocks it at a conservative 10 MHz; if the SD-shared bus proves flaky on
        // hardware, pin this to 10000000.
    // SD over SPI, sharing the display's SPI bus: SCLK13 / MOSI14 / MISO12 (the
    // vendor demo's pin_config.h confirms these as the EPD bus pins), SD_CS8,
    // SD_PWR_EN10. SD bus-sharing is inferred (the demo doesn't exercise SD) —
    // verify CS/transactions don't collide with the panel on hardware.
    {13, 12, 14, 8, 10, false, 0},
    // up5 down6; OK/confirm == power button GPIO4 (vendor demo: PIN_BTN_OK = PIN_POWER_BTN).
    // back/left/right come from touch. Active-low (10K pull-ups to VDD_3V3, button to GND).
    {PIN_UNASSIGNED, 4, PIN_UNASSIGNED, PIN_UNASSIGNED, 5, 6, 4, false},
    PIN_UNASSIGNED,  // batteryAdc: none — uses the I2C fuel gauge below
    40,              // batteryChargeStatus: CHARGE_STATE GPIO40 (from BQ25616)
    2.0f,
    PIN_UNASSIGNED,  // usbDetect: PWR_IN_VOLT (GPIO9 ADC) is board-support, not a digital detect
    // GT911 touch on its own I2C bus (SDA3 SCL2 INT21 RST41, 0x5D alt 0x14). GT911
    // reports pixel coords, so raw range == panel size; standard datasheet frame
    // layout (RST wired -> reset/config dance runs, track-id present).
    // gt911CoordsAtByte0=true: this panel's GT911 reports coords at byte 0 (no
    // track-id), like M5Paper — confirmed by raw point dumps during bring-up.
    // Portrait digitizer on a landscape panel: swapXY + flip both maps the sensor
    // frame onto the panel-native frame (confirmed by corner + menu bring-up taps).
    // rawMax* are the panel axes (post-swap). powerEnable=GPIO42 (TOUCH_EN).
    {TouchController::Gt911, 3, 2, 21, 41, 0x5D, 0, 799, 0, 479, false, 0x14, false, true, 42, true, true, true},
    NO_FRONTLIGHT,  // e-paper, no frontlight (charge LED is board-support)
    STICKY_AUDIO,   // no output codec; LEDC buzzer on GPIO48 (Buzzer lib). PDM mic is separate (mic field)
    NO_LEDS,        // charge-state LED is charger-driven, not an addressable strip
    NO_FLIP,        // mount orientation pending validation; see note above
    NO_SDMMC,       // SD is SPI, not 4-bit SDMMC
    // BQ27220 fuel gauge at 0x55 on the BFG/MISC I2C bus: SDA=GPIO1, SCL=GPIO0.
    // NOTE: GPIO0 is an ESP32-S3 strapping pin — the board init must not leave a
    // pull state that corrupts boot mode. No I2C charger (BQ25616 status is GPIO40).
    // Bus 1 (Wire1): the GT911 touch above owns Wire (SDA3/SCL2); the gauge is on a
    // separate physical bus, so it gets the second I2C controller to avoid a clash.
    {1, 0, 400000, 0x55, 0, 1},
    // Microphone: PDM mic (MSM261DDB020) — PDM_CLK GPIO19, PDM_DATA GPIO20, mic
    // power/enable (PDM_EN) GPIO38 (active-high via a load switch).
    {MicInput::Pdm, 19, 20, 38, true},
    // Sensors on the shared sensor I2C bus (SDA1/SCL0, same as the fuel gauge):
    // PCF8563 RTC (0x51), SHT40 temp/humidity (0x44), LSM6DS3TR-C IMU (0x6A).
    {1, 0, 400000, 0x51, 0x44, 0x6A, 1, RtcType::Pcf8563, ImuType::Lsm6ds3},
    1.2f,  // uiScale: touch device, 3.97" 800x480 — bump chrome to finger size
    // Power latch: PWR_HOLD GPIO45 + PWR_LOCK GPIO46, driven HIGH first thing in
    // boot (the vendor demo's first init step) — see holdPowerRails().
    {45, 46}};


