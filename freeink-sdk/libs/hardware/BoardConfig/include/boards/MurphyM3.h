#pragma once



// Murphy M3 audio, recovered from the OEM firmware: ES8388-compatible codec at
// 7-bit I2C 0x10 on the shared touch bus (SDA=13/SCL=12, 100 kHz), I2S master
// on BCLK=40/WS=39/DOUT=41/MCLK=42 (DIN unused). GPIO43 is driven HIGH by the
// stock board init and is preserved here as the enable line (not proven to be
// audio-specific, but the OEM bring-up notes say keep it high). GPIO46 carries
// a separate LEDC tone/buzzer path. No separate amp-enable pin.
constexpr AudioConfig MURPHY_AUDIO = {AudioOutput::I2sEs8388, 40, 39, 41,   42, 43, true,
                                      PIN_UNASSIGNED,         13, 12, 0x10, 46};

// --- Murphy M3 (CrowPanel 3.7") — UC8253, CHSC6x touch, PWM frontlight --------
constexpr BoardProfile MURPHY_M3 = {
    Board::MurphyM3,
    "murphy_m3",
    InputStyle::DigitalFiveKey,
    DisplayController::UC8253,
    // Framebuffer is landscape 416x240: the panel is a 240x416 controller held
    // rotated 90°, and the Murphy driver rotates each plane into controller RAM.
    416,
    240,
    {4, 3, 5, 6, 7, 8, PIN_UNASSIGNED},
    0,  // displaySpiHz: 0 -> Murphy UC8253 driver default (4 MHz)
    {39, 13, 40, 10, PIN_UNASSIGNED, true, 0},
    {PIN_UNASSIGNED, 0, PIN_UNASSIGNED, PIN_UNASSIGNED, 1, 2, 0, false},
    9,               // batteryAdc: stock firmware samples analogRead(9) for battery voltage
    PIN_UNASSIGNED,  // batteryChargeStatus: not identified
    3.030303f,       // stock firmware scales ADC by 0.0016 / 0.33, implying a 1:0.33 divider
    PIN_UNASSIGNED,
    {TouchController::Chsc6x, 13, 12, 44, 45, 0x2e, 24, 224, 24, 398, false, 0, true, false},
    {48, 25000, 10, true},
    // NOTE: the SPI SD pin guess above (39/13/40) predates the OEM firmware
    // audio recovery and conflicts with the proven I2S pins (39/40/41/42) and
    // shared I2C (13). Audio is the verified owner of those pins.
    MURPHY_AUDIO,
    NO_LEDS,
    NO_FLIP,
    NO_SDMMC,
    NO_GAUGE};


