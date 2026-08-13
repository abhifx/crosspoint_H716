#pragma once



// --- Xteink X3 — ESP32-C3, UC8253 (792x528) ----------------------------------
// Same board/pinout as X4; differs only in panel controller + size. Selected at
// runtime (setDisplayX3) so one C3 binary drives both. Keeping it a real sibling
// profile means resolution comes from BoardProfile for X3 just like every other
// device — the panel driver never special-cases its own geometry.
constexpr BoardProfile XTEINK_X3 = {
    Board::XteinkX3,
    "xteink_x3",
    InputStyle::XteinkAdcLadder,
    DisplayController::UC8253,
    792,
    528,
    {8, 10, 21, 4, 5, 6, PIN_UNASSIGNED},
    20000000,  // displaySpiHz: 20 MHz = UC8253 datasheet max. UC8253 datasheet (UltraChip / Good Display),
               // features: "Clock rate up to 20MHz" (serial write timing TSCYCW).
               // (https://www.elecrow.com/download/product/DIE01237S/UC8253_Datasheet.pdf)
               // Witch Reader (a CrossPoint fork) ran a conservative 16 MHz; 20 MHz is in-spec and ~25% faster
               // on plane writes. Falls back to the driver's 16 MHz default if set to 0.
    // powerEnable=GPIO13 = the X3 SD-rail power switch (active-high; HIGH at boot
    // powers the card, the sleep path drives it LOW). Confirmed by X3 factory-firmware
    // RE: setup() does digitalWrite(13,HIGH); every deep-sleep does digitalWrite(13,LOW).
    // Without declaring it, powerDownRailsForSleep() has no X3 SD enable to cut, so the
    // card stays powered through sleep -> battery drain. Shares the display SPI bus
    // (SCLK 8 / MOSI 10); MISO 7, CS 12. NOTE: X4 uses the same GPIO13 but keeps it as
    // power.latch0 (driven by the consumer's sleep path), so it is left unchanged.
    {PIN_UNASSIGNED, 7, PIN_UNASSIGNED, 12, 13, false, 0},
    {0, 1, 2, 3, 4, 5, 3, false},
    0,
    PIN_UNASSIGNED,
    2.0f,
    20,
    NO_TOUCH,
    NO_FRONTLIGHT,
    NO_AUDIO,
    NO_LEDS,
    NO_FLIP,
    NO_SDMMC,
    {20, 0, 400000, 0x55, 0},  // BQ27220 fuel gauge (0x55) on SDA20/SCL0; no charger IC
    NO_MIC,
    {20, 0, 400000, 0x68, 0, 0x6B, 0, RtcType::Ds3231, ImuType::Qmi8658}};


