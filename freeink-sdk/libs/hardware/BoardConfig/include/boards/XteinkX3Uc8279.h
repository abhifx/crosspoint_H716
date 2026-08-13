#pragma once



// --- Xteink X3 (UC8279d run) — ESP32-C3, UC8279d (792x528) -------------------
// Newer X3 production units swap the UC8253 for a UC8279d ("d_B" silicon; the
// TFT-module UltraChip BWR part driven in KW mode) on the same board, glass and
// pinout. Everything except the panel controller is inherited from XTEINK_X3;
// which sibling is running is fingerprinted at boot via the XteinkDetect
// display-controller probe (UC8279 VER/FLG readback). UC8279 serial write
// timing is also rated to 20 MHz ("Clock rate up to 20MHz").
constexpr BoardProfile XTEINK_X3_UC8279 = {
    Board::XteinkX3Uc8279,
    "xteink_x3_uc8279",
    InputStyle::XteinkAdcLadder,
    DisplayController::UC8279,
    792,
    528,
    {8, 10, 21, 4, 5, 6, PIN_UNASSIGNED},
    20000000,
    {PIN_UNASSIGNED, 7, PIN_UNASSIGNED, 12, 13, false, 0},  // SD powerEnable=GPIO13 (active-high) — see XTEINK_X3
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
    {20, 0, 400000, 0x55, 0},
    NO_MIC,
    {20, 0, 400000, 0x68, 0, 0x6B, 0, RtcType::Ds3231, ImuType::Qmi8658}};


