#pragma once



// --- Xteink X4 Pro — ESP32-S3, SSD1677 (800x480) + GT911 touch + warm/cold frontlight ---
// Recovered from the OEM flash dump (x4pro_flash_dump.bin); full evidence and confidence
// levels in docs/xteink-x4pro-support.md. This is a DISTINCT device from the C3
// `XTEINK_X4` above: same panel controller/size, but an ESP32-S3 with 8 MB PSRAM, a
// GT911 capacitive digitizer, and a dual warm/cold color-temperature frontlight.
//
// Confidence summary:
//   CONFIRMED : display SPI + panel pins, GT911 controller/address, ADC-ladder input style.
//   HIGH      : GT911 I2C/INT/RST pins, SD SPI bus + CS (m_csPin=GPIO45) + enable GPIO5 (driven
//     HIGH), the BM8563 RTC (0x51 on the shared touch bus), and the GPIO1 master rail (driven
//     HIGH first in board init) — all from the board pin-init table at IROM 0x420a2240.
//   PENDING hardware validation: panel orientation (ships NO_FLIP), touch swap/flip, the exact
//     frontlight GPIO(s)/freq (warm+cold; the SDK models one channel — primary brightness here),
//     the GPIO5 SD-enable role and GPIO2 (a board-init output driven LOW, role unknown), and
//     battery/VBUS pins. The ADC-ladder pins are UNKNOWN — GPIO1/GPIO2 (the old guess) are power
//     outputs, not ladder inputs. See the findings doc before trusting any PENDING value.
constexpr BoardProfile XTEINK_X4_PRO = {
    Board::XteinkX4Pro,
    "xteink_x4_pro",
    InputStyle::DigitalButtons,  // confirmed on hardware: plain active-low GPIO buttons, not the OEM ADC ladder
    DisplayController::SSD1677,
    800,
    480,
    // SSD1677 SPI — CONFIRMED ON HARDWARE via a raw bit-banged pin sweep (the panel
    // painted with these and only these): SCLK=12 MOSI=11 (write-only, no MISO)
    // CS=13 DC=18 RST=14 BUSY=6. Note vs the RE guesses: SCLK/MOSI are app0's order
    // (the app1 RE's 11/12 was backwards) and CS/DC are swapped from app0's 18/13.
    // The plain X4 OTP waveform develops the image — no custom LUT/voltages/PMIC
    // needed. GPIO1 also triggers a refresh when toggled (likely a panel power
    // enable), but the panel works without driving it, so powerEnable stays unset.
    {12, 11, 13, 18, 14, 6, PIN_UNASSIGNED},
    20000000,  // displaySpiHz: 20 MHz, matching the X4's default. The OEM clocks the panel at only 5 MHz
               // (SPISettings 0x4C4B40), but the SSD1677 handles far more (X4 runs 20, de-link 40), so 20 MHz
               // is well in spec and gives noticeably faster RAM writes. Drop back to 5 MHz if artifacts appear.
    // SD is native SDMMC (see the sdmmc field below) — the card is silent to SPI-mode CMD0 on
    // hardware. This SPI SdPins entry is retained only for its powerEnable=GPIO5, the SD enable
    // used by the SDMMC mount path. GPIO5 is ACTIVE-LOW: SdmmcBlockDevice pulses it HIGH→LOW
    // before each mount attempt and runs the card with it held LOW (matching the OEM mountSD;
    // holding it HIGH breaks every block read with 0x107). The bus pins (SCLK41 MISO40 MOSI42
    // CS45) are the SPI view of the same slot and are unused now that busWidth!=0 routes through
    // the SDMMC block device. Trailing false = powerEnable is active-LOW, so the sleep path drives
    // GPIO5 HIGH to power the card down.
    {41, 40, 42, 45, 5, true, 0, false},
    // Digital buttons, confirmed on hardware (watch-up edge test): two physical nav keys —
    // Left=GPIO0, Right=GPIO7 — plus Power=GPIO3, all active-LOW (INPUT_PULLUP, no rail needed).
    // The two keys map to the reader's page pair (Up=prev / Down=next), so Left→up, Right→down;
    // back/confirm come from the GT911 (touch + the capacitive Home key). NOTE: GPIO0 is a boot
    // strap — fine as a button as long as it isn't held during reset.
    // {back, confirm, left, right, up, down, power, powerActiveHigh}
    {PIN_UNASSIGNED, 38, PIN_UNASSIGNED, PIN_UNASSIGNED, 0, 7, 3, false},
    PIN_UNASSIGNED,  // batteryAdc: monitoring exists ("Battery Meter"/"Low battery") but pin not isolated
    PIN_UNASSIGNED,  // batteryChargeStatus
    2.0f,
    PIN_UNASSIGNED,  // usbDetect: USB-MSC/VBUS-detect present; GPIO10 is a candidate (unconfirmed)
    // GT911 touch on the SHARED I2C bus SDA39/SCL38 (with RTC 0x51 + CW2017 gauge 0x63), addr 0x5D
    // (alt 0x14), 400 kHz. CONFIRMED ON HARDWARE: **INT=GPIO10, RST=GPIO4** (a first RE had these
    // reversed), and the controller is on an **active-LOW power rail: GPIO2** (powerEnable=2,
    // powerEnableActiveHigh=false) — the GT911 stays unpowered/silent until GPIO2 is driven LOW
    // (GPIO1, power.latch0, must also be HIGH). Like the Sticky panel, the GT911 SELF-LOADS its
    // internal config on the standard reset dance — no host config upload needed. Mounted PORTRAIT
    // (reports X:0..480, Y:0..800) on the 800x480 landscape panel → swapXY=true; rawMax describe the
    // post-swap panel axes. Coords start at byte 0 of the 0x8150 read → gt911CoordsAtByte0=true.
    // flipX/flipY pending a corner-tap test. {ctrl,sda,scl,irq,rst,addr,rawMinX,rawMaxX,rawMinY,rawMaxY,
    //  synthConfirm,altAddr,irqActiveLow,coordsAtByte0,powerEnable,swapXY,flipX,flipY,hasHomeKey,pwrActiveHigh}
    {TouchController::Gt911, 39, 38, 10, 4, 0x5D, 0, 799, 0, 479, false, 0x14, false, true, 2,
     true, false, true, true, false},  // swapXY + flipY (confirmed by corner-tap); powerEnable=GPIO2 active-LOW; hasHomeKey
    // Frontlight: dual warm/cold LEDC PWM with color temperature (NVS lightWarmValue/
    // lightColdValue/lightCT/lightBri/lightOn). Recovered from the OEM LEDC init (IROM
    // 0x420a2130 → helper 0x420a20c0): two channels — GPIO8 on LEDC ch4 and GPIO9 on ch5 —
    // both at 10 kHz / 10-bit, active-HIGH (init drives the pin LOW = off, brightness raises
    // duty). The SDK's FrontlightConfig models ONE channel, so this carries GPIO8 as the
    // primary brightness pin, GPIO9 as the warm channel — FrontlightManager mixes them for
    // color-temperature control. Which of GPIO8/GPIO9 is physically warm vs cold is not yet
    // known; if reversed, the CT direction just inverts (user-flippable).
    {8, 10000, 10, true, 9},
    NO_AUDIO,
    NO_LEDS,
    NO_FLIP,  // panel mount transform pending hardware; native SSD1677 scan is 800x480 landscape
    // SD is native SDMMC, NOT SPI: the card the OEM reads is silent to SPI-mode CMD0.
    // CONFIRMED on hardware: 1-bit, slot 1, CLK=41 CMD=42 DAT0=40, internal pull-ups, 40 MHz.
    // D1/D2/D3 are UNUSED in 1-bit. Mounts reliably via SdmmcBlockDevice, which power-cycles
    // the GPIO5 enable (see the SPI SdPins powerEnable above) and validates a real sector-0
    // read per attempt. {clk,cmd,d0,d1,d2,d3,busWidth}
    {41, 42, 40, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, 1},
    // CW2017 fuel gauge at I2C 0x63 on the SHARED touch/RTC bus SDA39/SCL38, 400 kHz, Wire.
    // BatteryMonitor uploads the 80-byte BATINFO battery profile (recovered from app1's
    // XTEink Cw2017PowerHal via Ghidra) if the gauge hasn't got one, then reads SoC from
    // reg 0x04. No charger IC on the gauge bus. {sda,scl,hz,gaugeAddr,chargerAddr,bus,type}
    {39, 38, 400000, 0x63, 0, 0, GaugeType::Cw2017},
    NO_MIC,
    // BM8563 RTC (PCF8563 register-compatible, class XTEink::BM8563Driver in the dump) at I2C
    // 0x51, sharing the GT911 touch bus SDA39/SCL38 at 400 kHz (recovered: driver init at IROM
    // 0x420a2834 adds device 0x51; the bus object is configured with {39,38,400000}). Bus 0
    // (Wire), matching the touch driver so both drive the same peripheral on the shared pins.
    {39, 38, 400000, 0x51, 0, 0, 0, RtcType::Pcf8563, ImuType::None},  // temp/hum + IMU: none
    1.2f,  // uiScale: 800x480 touch device — finger-sized chrome, like the other touch boards
    // Master peripheral-rail enable on GPIO1: the OEM board-init drives it HIGH first, before
    // any SPI/display/SD bring-up (recovered: standalone OUTPUT, level=1, acted on first in
    // board_begin at IROM 0x420a23dc). Carried as power.latch0 so holdPowerRails() asserts it
    // early — without it the panel rail and the SD slot both stay unpowered (the bring-up
    // symptom: EPD BUSY never asserts, SD returns 0xFF). GPIO2 is a second board-init output
    // driven LOW (role unknown); not modeled here. NOTE: GPIO1/GPIO2 are therefore NOT the ADC
    // button ladder — that earlier assumption was wrong; the ladder pins remain unconfirmed.
    {1}};


