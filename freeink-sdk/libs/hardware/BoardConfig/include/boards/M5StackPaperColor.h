#pragma once



// M5 PaperColor audio, from the official pin map (docs.m5stack.com/en/core/
// PaperColor) and M5Unified's speaker bring-up: ES8311 mono codec at 7-bit I2C
// 0x18 on the system bus (SDA=3/SCL=2 — shared with the M5PM1 PMIC, same
// 100 kHz), I2S master on BCLK=40/WS=41/DOUT=38. The MCLK line (GPIO42) is
// deliberately left unwired: like M5Unified, the codec derives its clock from
// BCLK (reg 0x01=0xB5 / 0x02=0x18), which makes the init sample-rate-agnostic.
// GPIO45 (AUDIO_PWR_EN) powers the codec/mic rail; GPIO46 (SPK_EN) enables the
// AW8737A speaker amp and is raised only while playing. The ES7210 mic ADC
// (0x40) is not driven.
constexpr AudioConfig M5_PAPERCOLOR_AUDIO = {
    AudioOutput::I2sEs8311, 40, 41, 38, PIN_UNASSIGNED, 45, true, 46, 3, 2, 0x18, PIN_UNASSIGNED};

constexpr LedConfig M5_PAPERCOLOR_LEDS = {21, 2, LedColorOrder::GRB, true};  // bench-verified GRB

// --- M5Stack PaperColor — ESP32-S3, ED2208 color panel, M5PM1 PMIC -----------
constexpr BoardProfile M5STACK_PAPER_COLOR = {Board::M5StackPaperColor,
                                              "m5stack_papercolor",
                                              InputStyle::DigitalConfirmBackHold,
                                              DisplayController::ED2208,
                                              400,
                                              600,
                                              {15, 13, 44, 43, 12, 11, PIN_UNASSIGNED},
                                              0,  // displaySpiHz: 0 -> ED2208 driver default (4 MHz)
                                              {15, 14, 13, 47, PIN_UNASSIGNED, false, 0},
                                              {1, 1, PIN_UNASSIGNED, PIN_UNASSIGNED, 10, 9, 1, false},
                                              PIN_UNASSIGNED,
                                              PIN_UNASSIGNED,
                                              2.0f,
                                              PIN_UNASSIGNED,
                                              NO_TOUCH,
                                              NO_FRONTLIGHT,
                                              M5_PAPERCOLOR_AUDIO,
                                              M5_PAPERCOLOR_LEDS,
                                              NO_FLIP,
                                              NO_SDMMC,
                                              NO_GAUGE};


