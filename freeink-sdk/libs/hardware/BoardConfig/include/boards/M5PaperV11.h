#pragma once



// --- M5Paper v1.1 4.7" (ED047TC1 behind an IT8951E controller) — ESP32 --------
// 540x960 16-gray panel driven through an IT8951E timing controller over SPI
// (MOSI12 MISO13 SCLK14 CS15, HRDY/busy GPIO27, EPD power-enable GPIO23). The
// framebuffer is landscape 960x540 (byte-aligned; 540 is not a multiple of 8) and
// the IT8951 driver rotates it onto the portrait panel — the rotation is an
// injectable driver-config field, so a board that mounts the panel differently
// flips it without code changes. GT911 touch (I2C SDA21/SCL22, INT36) is reused
// from InputManager. Battery is read on the GPIO35 ADC. The 3-position rotary
// switch maps push=CONFIRM(38), left(39), right(37).
//
// System note: M5Paper latches its own power through a MOSFET on GPIO2 — it must
// be driven HIGH at boot or the device powers off the moment USB is unplugged.
// The profile's power.latch0 carries it, asserted by holdPowerRails() (call it
// first thing in setup(), like the Sticky). EXT power (GPIO5) and EPD power
// (GPIO23) gate the peripheral and panel rails; the IT8951 driver asserts GPIO23
// (the EPD rail) itself.
constexpr BoardProfile M5PAPER_V11 = {
    Board::M5PaperV11,
    "m5paper_v11",
    InputStyle::DigitalButtons,
    DisplayController::IT8951,
    960,  // landscape framebuffer (byte-aligned); driver rotates onto the 540x960 panel
    540,
    {14, 12, 15, PIN_UNASSIGNED, PIN_UNASSIGNED, 27, 23},  // SCLK14 MOSI12 CS15, no DC/RST, HRDY27, EPD_PWR_EN23
    0,                                                     // displaySpiHz: 0 -> IT8951 driver default (10 MHz)
    {14, 13, 12, 4, PIN_UNASSIGNED, false, 20000000},      // SD shares the SPI bus: SCLK14 MISO13 MOSI12, CS4. 20 MHz
                                                           // (not the 40 MHz default): the bus is shared with the EPD,
                                                           // and 40 MHz gives SdFat READ_TIMEOUT on the CSD/data read.
    // Rotary wheel is M5Paper's only button input (3 positions: 37, push=38, 39,
    // active-low via external pull-ups). The two sides MUST drive page navigation —
    // CrossPoint's reader pages on BTN_UP/BTN_DOWN (the fixed side buttons), so
    // up=37, down=39 (matches the physical wheel orientation; direction is also
    // user-swappable in settings). The push (38) is CONFIRM and doubles as the
    // power/wake button: 38 is an RTC GPIO, so it's the ext1 deep-sleep wake source.
    // Back/Left/Right have no GPIO (only 3 wheel inputs) — M5Paper uses its GT911
    // touch for those. So confirm and power share pin 38 by design.
    {PIN_UNASSIGNED, 38, PIN_UNASSIGNED, PIN_UNASSIGNED, 37, 39, 38, false},
    35,  // batteryAdc GPIO35 (2:1 divider; pending hardware validation)
    PIN_UNASSIGNED,
    2.0f,
    PIN_UNASSIGNED,
    // GT911 touch, shared I2C SDA21/SCL22, INT36, 0x5D (alt 0x14), no reset GPIO.
    // The GT911 reports in the silicon's native PORTRAIT frame (540x960), but the IT8951
    // driver rotates the 960x540 framebuffer 90° onto that silicon, so the renderer + tap
    // pipeline (GfxRenderer::tapToLogical) work in the 960x540 FRAMEBUFFER frame. Align
    // touch to that frame like every other board (cf. Sticky): swapXY=true with rawMax in
    // FB-landscape order (959 x 539). Without the swap the tap normalizes over 540x960 while
    // tapToLogical scales by 960x540 — aspect/axis-swapped coords: the back corner still
    // roughly hits but mid-screen taps are way off (worst along the tall 960 axis).
    // flipY=true keeps the back gesture upright (silicon (0,0) = physical top-left, from the
    // working back-gesture corner); flipX=false (X was never mirrored).
    {TouchController::Gt911, 21, 22, 36, PIN_UNASSIGNED, 0x5D, 0, 959, 0, 539, false, 0x14, false,
     true,  // gt911CoordsAtByte0: reports coords at byte 0 (no track-id) on M5Paper
     PIN_UNASSIGNED, true, false, true},  // powerEnable, swapXY=true, flipX=false, flipY=true
    NO_FRONTLIGHT,
    NO_AUDIO,
    NO_LEDS,
    NO_FLIP,
    NO_SDMMC,
    NO_GAUGE,
    NO_MIC,
    NO_SENSORS,
    1.2f,  // uiScale: 4.7" 960x540 touch (~234 PPI) — finger-sized chrome, like Sticky
    // Power latch: main-power MOSFET on GPIO2, driven HIGH first thing in boot
    // via holdPowerRails() or the board powers off when USB is unplugged.
    {2}};


