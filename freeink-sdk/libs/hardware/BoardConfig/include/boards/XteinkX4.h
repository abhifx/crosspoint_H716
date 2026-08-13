#pragma once



// --- Xteink X4 — ESP32-C3, SSD1677 (800x480) ---------------------------------
// X4 display SPI clock. Default 20 MHz = SSD1677 datasheet max for write mode
// (Solomon Systech SSD1677, MCU Serial Interface AC Characteristics: "MCU
// interface: SPI serial peripheral, Maximum 20MHz for write"; fSCL Write = 20 MHz.
// https://files.waveshare.com/upload/2/2a/SSD1677_1.0.pdf). The plane writes are
// ~38 ms/refresh at 20 MHz. Define -DFREEINK_X4_OVERCLOCK_SPI to run 40 MHz — the
// (out-of-spec, 2x datasheet) clock the CrossPoint / Witch Reader fork used, which
// halves that to ~19 ms (~17-20 ms/refresh faster) but can glitch plane writes on
// marginal wiring. Opt-in only; validate on your hardware. (NB: the Ssd1677Driver
// 0-default of 40 MHz is likewise over spec for boards that leave displaySpiHz 0.)
#ifdef FREEINK_X4_OVERCLOCK_SPI
#define FREEINK_X4_DISPLAY_SPI_HZ 40000000u
#else
#define FREEINK_X4_DISPLAY_SPI_HZ 20000000u
#endif

constexpr BoardProfile XTEINK_X4 = {Board::XteinkX4,
                                    "xteink_x4",
                                    InputStyle::XteinkAdcLadder,
                                    DisplayController::SSD1677,
                                    800,
                                    480,
                                    {8, 10, 21, 4, 5, 6, PIN_UNASSIGNED},
                                    FREEINK_X4_DISPLAY_SPI_HZ,  // displaySpiHz — see FREEINK_X4_OVERCLOCK_SPI above
                                    {PIN_UNASSIGNED, 7, PIN_UNASSIGNED, 12, PIN_UNASSIGNED, false, 0},
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
                                    NO_GAUGE,
                                    NO_MIC,
                                    NO_SENSORS,
                                    1.0f,
                                    // GPIO13 gates the battery MOSFET. Known units self-latch through a
                                    // pull once the power button bridges the rail, so firmware never had
                                    // to assert it — but at least one hardware revision in the field does
                                    // not self-latch and stays powered only while the button is held.
                                    // Asserting the latch is a no-op on self-latching units. Driving it
                                    // LOW is the battery power-off (see consumers' deep-sleep path).
                                    {13, PIN_UNASSIGNED}};


