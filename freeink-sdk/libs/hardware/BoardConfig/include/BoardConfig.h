#pragma once

// FreeInk SDK — board hardware profiles + build composition.
//
// A BoardProfile describes a device's pinout, screen, and capabilities. The
// runtime-active profile is BoardConfig::ACTIVE; drivers (display / input /
// power) read from it so the same code adapts to any board.
//
// A build is composed along two axes:
//   * DEVICES   (-DFREEINK_DEVICE_<NAME>) — which hardware the binary supports.
//   * CAPABILITIES (-DFREEINK_CAP_<NAME>) — which feature code is compiled in.
//
// Devices that share a binary must share an MCU and (to be runtime-selected)
// supply their own detection in the consumer. X3 and X4 are two profiles in one
// ESP32-C3 binary, picked at runtime via EInkDisplay::setDisplayX3() (which calls
// selectDevice); ACTIVE defaults to a compile-time default until then.

#include <Arduino.h>
#include <driver/gpio.h>   // gpio_hold_dis in releaseSdRail()
#include <esp_rom_sys.h>  // esp_rom_printf in holdPowerRails()

// ============================================================================
// Build composition — devices x capabilities
// ============================================================================

// --- 1) Devices are selected explicitly --------------------------------------
// A build declares its hardware with one or more -DFREEINK_DEVICE_<NAME> in its
// platformio env (see platformio.sample.ini). There is no default and no
// inference from board macros — pick your device(s) by setting the flag(s). The
// coherence check below errors if none (or an incompatible mix) is selected.

// Normalize device flags to 0/1.
#ifndef FREEINK_DEVICE_X4
#define FREEINK_DEVICE_X4 0
#endif
#ifndef FREEINK_DEVICE_X3
#define FREEINK_DEVICE_X3 0
#endif
#ifndef FREEINK_DEVICE_X4PRO
#define FREEINK_DEVICE_X4PRO 0
#endif
#ifndef FREEINK_DEVICE_M5
#define FREEINK_DEVICE_M5 0
#endif
#ifndef FREEINK_DEVICE_MURPHY
#define FREEINK_DEVICE_MURPHY 0
#endif
#ifndef FREEINK_DEVICE_DELINK
#define FREEINK_DEVICE_DELINK 0
#endif
#ifndef FREEINK_DEVICE_LILYGO
#define FREEINK_DEVICE_LILYGO 0
#endif
#ifndef FREEINK_DEVICE_LILYGO_H716
#define FREEINK_DEVICE_LILYGO_H716 0
#endif
#ifndef FREEINK_DEVICE_M5PAPER
#define FREEINK_DEVICE_M5PAPER 0
#endif
#ifndef FREEINK_DEVICE_STICKY
#define FREEINK_DEVICE_STICKY 0
#endif

// --- 2) Coherence: exactly one MCU family, at least one device ---------------
#if !(FREEINK_DEVICE_X4 || FREEINK_DEVICE_X3 || FREEINK_DEVICE_X4PRO || FREEINK_DEVICE_M5 || FREEINK_DEVICE_MURPHY || \
      FREEINK_DEVICE_DELINK || FREEINK_DEVICE_LILYGO || FREEINK_DEVICE_LILYGO_H716 || FREEINK_DEVICE_M5PAPER || FREEINK_DEVICE_STICKY)
#error \
    "FreeInk: no device selected. Pass at least one -DFREEINK_DEVICE_<NAME> (X4, X3, X4PRO, M5, MURPHY, DELINK, LILYGO, LILYGO_H716, M5PAPER, STICKY) in your build env — see platformio.sample.ini."
#endif
// Each device belongs to one MCU family; a binary targets exactly one. X3/X4 are
// ESP32-C3; M5 PaperColor/Murphy/de-link/LilyGo are ESP32-S3; M5Paper v1.1 is the
// classic ESP32 (ESP32-D0WDQ6). The three families differ in deep-sleep wakeup,
// SPI peripheral count, and toolchain, so they never share a binary.
#define FREEINK_MCU_C3 (FREEINK_DEVICE_X3 || FREEINK_DEVICE_X4)
#define FREEINK_MCU_S3                                                                                    \
  (FREEINK_DEVICE_M5 || FREEINK_DEVICE_MURPHY || FREEINK_DEVICE_DELINK || FREEINK_DEVICE_LILYGO ||        \
   FREEINK_DEVICE_LILYGO_H716 || FREEINK_DEVICE_STICKY || FREEINK_DEVICE_X4PRO)
#define FREEINK_MCU_ESP32 (FREEINK_DEVICE_M5PAPER)
#if (FREEINK_MCU_C3 + FREEINK_MCU_S3 + FREEINK_MCU_ESP32) != 1
#error \
    "FreeInk: all selected devices must share one MCU family — ESP32-C3 (X3/X4), ESP32-S3 (M5/Murphy/de-link/LilyGo/Sticky/X4Pro), or ESP32 (M5Paper). Build one binary per family."
#endif

// --- 3) Derive panel drivers from the device set -----------------------------
// Sticky reuses SSD1677: its 800x480 panel rides a 24-pin FPC whose GDR/RESE/BS1
// + dual VSH1/VSH2 + external VGH/VGL/VSL/VCOM charge pump is the SSD1677
// application circuit (same controller + resolution as X4 / de-link).
// X4 Pro is a distinct ESP32-S3 device (NOT the C3 X4): same SSD1677 controller and
// 800x480 panel as X4/de-link/Sticky, recovered from its OEM firmware dump — see
// docs/xteink-x4pro-support.md.
#if FREEINK_DEVICE_X4 || FREEINK_DEVICE_DELINK || FREEINK_DEVICE_STICKY || FREEINK_DEVICE_X4PRO
#define FREEINK_DRIVER_SSD1677 1
#else
#define FREEINK_DRIVER_SSD1677 0
#endif
#if FREEINK_DEVICE_X3
#define FREEINK_DRIVER_UC8253_X3 1
#else
#define FREEINK_DRIVER_UC8253_X3 0
#endif
// UltraChip controller variants. Newer batches of several Xteink panels ship an
// UltraChip controller in place of the original. Both are in the UC81xx KW
// command family but are separate drivers (different power/LUT bring-up):
//   * UC8279d — X3 (792x528), replaces the UC8253. Runs pure OTP waveforms.
//   * UC8179  — X4 / X4 Pro (800x480), replaces the SSD1677. Needs an explicit
//     PLL/booster/VCOM bring-up.
//   * UC8279 (800x480) — a second UltraChip variant of the X4 Pro panel
//     (LUT_VER 0x02/0x68); its own driver (different PSR/PLL init, 1-byte CDI,
//     gate offset, inverted AA planes).
// Which controller a given unit runs is resolved at boot by the display-bus
// probe (0x70 VER readback; NVS hw_calib/screenType is diagnostics-only) and the
// matching driver is selected before display begin(). Link each driver wherever
// a batch might carry it.
#if FREEINK_DEVICE_X3
#define FREEINK_DRIVER_UC8279 1
#else
#define FREEINK_DRIVER_UC8279 0
#endif
#if FREEINK_DEVICE_X4 || FREEINK_DEVICE_X4PRO
#define FREEINK_DRIVER_UC8179 1
#define FREEINK_DRIVER_UC8279_X4 1
#else
#define FREEINK_DRIVER_UC8179 0
#define FREEINK_DRIVER_UC8279_X4 0
#endif
// M5 PaperColor has two interchangeable display backends: the fast hand-rolled
// ED2208 driver (default), or M5's official M5GFX/M5Unified path (opt in with
// -DFREEINK_M5_OFFICIAL=1, which pulls the M5 libraries — see platformio.sample).
#if FREEINK_DEVICE_M5 && defined(FREEINK_M5_OFFICIAL) && FREEINK_M5_OFFICIAL
#define FREEINK_DRIVER_M5_OFFICIAL 1
#define FREEINK_DRIVER_ED2208 0
#elif FREEINK_DEVICE_M5
#define FREEINK_DRIVER_ED2208 1
#define FREEINK_DRIVER_M5_OFFICIAL 0
#else
#define FREEINK_DRIVER_ED2208 0
#define FREEINK_DRIVER_M5_OFFICIAL 0
#endif
#if FREEINK_DEVICE_MURPHY
#define FREEINK_DRIVER_UC8253_MURPHY 1
#else
#define FREEINK_DRIVER_UC8253_MURPHY 0
#endif
// LilyGo T5 S3: raw-parallel ED047TC1 via LovyanGFX (M5GFX). External-bus driver.
#if FREEINK_DEVICE_LILYGO || FREEINK_DEVICE_LILYGO_H716
#define FREEINK_DRIVER_LGFX_EPD 1
#else
#define FREEINK_DRIVER_LGFX_EPD 0
#endif
// M5Paper v1.1: ED047TC1 behind an IT8951E timing controller (its own framebuffer
// SRAM, 16-bit-word SPI with MISO reads). The driver owns its SPI end to end.
#if FREEINK_DEVICE_M5PAPER
#define FREEINK_DRIVER_IT8951 1
#else
#define FREEINK_DRIVER_IT8951 0
#endif

// --- 4) Derive default capabilities (override with -DFREEINK_CAP_*=0/1) -------
#ifndef FREEINK_CAP_TOUCH
#define FREEINK_CAP_TOUCH                                                                         \
  (FREEINK_DEVICE_MURPHY || FREEINK_DEVICE_LILYGO || FREEINK_DEVICE_LILYGO_H716 || FREEINK_DEVICE_M5PAPER || \
   FREEINK_DEVICE_STICKY || FREEINK_DEVICE_X4PRO)
#endif
#ifndef FREEINK_CAP_FRONTLIGHT
#define FREEINK_CAP_FRONTLIGHT \
  (FREEINK_DEVICE_DELINK || FREEINK_DEVICE_MURPHY || FREEINK_DEVICE_LILYGO || FREEINK_DEVICE_LILYGO_H716 || \
   FREEINK_DEVICE_X4PRO)
#endif
// USB Mass Storage ("USB Transfer" mode): exposes the SD card to a host over
// USB-MSC. OPT-IN (default off), NOT board-derived: it forces the build into
// USB-OTG mode (ARDUINO_USB_MODE=0 + CONFIG_TINYUSB_MSC_ENABLED), which changes
// how the USB serial console works — so a board enables it in its OWN env
// alongside those flags (e.g. X4 Pro adds -DFREEINK_CAP_USB_MSC=1
// -DARDUINO_USB_MODE=0 -DARDUINO_USB_CDC_ON_BOOT=1). Native-USB (ESP32-S3/C3
// OTG) targets only. When 0, UsbMassStorage links stub bodies and pulls in no
// TinyUSB/MSC code. Requires SDMMC/SPI storage exposing a block device.
#ifndef FREEINK_CAP_USB_MSC
#define FREEINK_CAP_USB_MSC 0
#endif
// BLE HID host. The BleKeyboardHost lib pairs/connects to Bluetooth Low Energy
// HID peripherals such as keyboards and page turners and emits translated key
// events; it compiles its NimBLE central code only when this is set, otherwise
// it links stub bodies and pulls in no BLE code at all. Default off: it's an
// opt-in feature, not board-derived. ESP32-C3/S3 targets only (BLE required).
#ifndef FREEINK_CAP_BLE_HID_HOST
#ifdef FREEINK_CAP_BLE_KEYBOARD
#define FREEINK_CAP_BLE_HID_HOST FREEINK_CAP_BLE_KEYBOARD
#else
#define FREEINK_CAP_BLE_HID_HOST 0
#endif
#endif
#ifndef FREEINK_CAP_BLE_KEYBOARD
#define FREEINK_CAP_BLE_KEYBOARD FREEINK_CAP_BLE_HID_HOST
#endif
// Scan-list policy for the BLE HID host. Default hides anonymous non-HID
// advertisers so firmware pairing UIs are not filled with random beacon
// addresses. Set -DFREEINK_BLE_HID_SHOW_UNNAMED_DEVICES=1 during bring-up to
// include connectable unnamed devices as probe candidates. Devices advertising
// HID are always kept, even without a name.
#ifndef FREEINK_BLE_HID_SHOW_UNNAMED_DEVICES
#define FREEINK_BLE_HID_SHOW_UNNAMED_DEVICES 0
#endif
// Security policy for BLE HID host pairing. Default to Just Works bonding
// because many page-turner remotes have no input/display capability and reject
// mandatory MITM/passkey pairing. Firmware that specifically wants keyboard
// passkey pairing can opt in with -DFREEINK_BLE_HID_REQUIRE_MITM=1.
#ifndef FREEINK_BLE_HID_REQUIRE_MITM
#define FREEINK_BLE_HID_REQUIRE_MITM 0
#endif

// I2C fuel-gauge battery backend. Compiled in when a build contains a gauge
// device (X3's BQ27220, or LilyGo's BQ27220+BQ25896). Selection is then *runtime*
// per active profile (BatteryMonitor uses the gauge only when
// ACTIVE.batteryGauge.gaugeAddr != 0) — required because X3 (gauge) and X4 (ADC)
// share one C3 binary.
#ifndef FREEINK_BATTERY_I2C_GAUGE
#define FREEINK_BATTERY_I2C_GAUGE \
  (FREEINK_DEVICE_X3 || FREEINK_DEVICE_LILYGO || FREEINK_DEVICE_LILYGO_H716 || FREEINK_DEVICE_STICKY || \
   FREEINK_DEVICE_X4PRO)
#endif
#ifndef FREEINK_CAP_COLOR
#define FREEINK_CAP_COLOR (FREEINK_DEVICE_M5)
#endif
#ifndef FREEINK_CAP_AUDIO
#define FREEINK_CAP_AUDIO (FREEINK_DEVICE_MURPHY || FREEINK_DEVICE_M5)
#endif
// Microphone capture (PDM in). Separate from FREEINK_CAP_AUDIO (output): the
// Sticky has a PDM mic but no output codec. The Microphone lib compiles its
// i2s_pdm RX path only when this is set; otherwise it links stub bodies.
#ifndef FREEINK_CAP_MIC
#define FREEINK_CAP_MIC (FREEINK_DEVICE_STICKY)
#endif
// On-board I2C sensors. Each lib (Rtc / EnvironmentSensor / Imu) compiles its
// I2C driver only when its flag is set; otherwise it links stub bodies.
#ifndef FREEINK_CAP_RTC
#define FREEINK_CAP_RTC (FREEINK_DEVICE_X3 || FREEINK_DEVICE_STICKY || FREEINK_DEVICE_X4PRO || FREEINK_DEVICE_LILYGO_H716)
#endif
#ifndef FREEINK_CAP_TEMP_HUMIDITY
#define FREEINK_CAP_TEMP_HUMIDITY (FREEINK_DEVICE_STICKY)
#endif
#ifndef FREEINK_CAP_IMU
#define FREEINK_CAP_IMU (FREEINK_DEVICE_X3 || FREEINK_DEVICE_STICKY)
#endif
// LEDC PWM buzzer (tone beeper). The Buzzer lib drives the AudioConfig.buzzer
// pin; on for boards that wire one (Sticky GPIO48, Murphy GPIO46). Separate from
// FREEINK_CAP_AUDIO — a buzzer is a tone device, not a WAV/codec output.
#ifndef FREEINK_CAP_BUZZER
#define FREEINK_CAP_BUZZER (FREEINK_DEVICE_STICKY || FREEINK_DEVICE_MURPHY)
#endif
#ifndef FREEINK_CAP_LED
#define FREEINK_CAP_LED (FREEINK_DEVICE_M5)
#endif
#ifndef FREEINK_CAP_NET_TLS13
#if defined(FREEINK_NET_WOLFSSL)
#define FREEINK_CAP_NET_TLS13 1
#else
#define FREEINK_CAP_NET_TLS13 0
#endif
#endif

// Place the facade framebuffer(s) in PSRAM (heap, MALLOC_CAP_SPIRAM) instead of
// static DRAM .bss. Default on for M5Paper v1.1: the classic ESP32 has tight
// internal DRAM but 8MB PSRAM, and the 63KB 540x960 framebuffer does not fit in
// .bss alongside the firmware. Every other device keeps the static DRAM array.
// (The prebuilt Arduino-ESP32 libs disable BSS-in-PSRAM, so this is a runtime
// heap allocation, not EXT_RAM_BSS_ATTR.)
#ifndef FREEINK_FB_PSRAM
#define FREEINK_FB_PSRAM (FREEINK_DEVICE_M5PAPER)
#endif

// SD transport. de-link (4-bit) and X4 Pro (1-bit) are wired for SDMMC; SdFat
// can't drive SDIO, so they get a native esp-idf SDMMC block device behind
// SDCardManager. Every other board stays on SdFat-over-SPI. The consumer's build
// must define USE_BLOCK_DEVICE_INTERFACE=1 for the SdFat FsVolume these mount on.
// Override with -DFREEINK_SD_SDMMC=0/1.
#ifndef FREEINK_SD_SDMMC
#define FREEINK_SD_SDMMC (FREEINK_DEVICE_DELINK || FREEINK_DEVICE_X4PRO)
#endif

// Serial log transport hint for consumer firmware. Boards can share the same MCU
// but expose logs differently: LilyGo T5 S3 is monitored over native USB CDC,
// while Sticky bring-up is more reliable through the IDF/ROM console path.
#define FREEINK_LOG_TRANSPORT_SERIAL 0
#define FREEINK_LOG_TRANSPORT_USB_CDC_WRITE 1
#define FREEINK_LOG_TRANSPORT_ROM_PRINTF 2
#ifndef FREEINK_LOG_TRANSPORT
#if FREEINK_DEVICE_LILYGO || FREEINK_DEVICE_LILYGO_H716
#define FREEINK_LOG_TRANSPORT FREEINK_LOG_TRANSPORT_USB_CDC_WRITE
#elif FREEINK_DEVICE_STICKY
#define FREEINK_LOG_TRANSPORT FREEINK_LOG_TRANSPORT_ROM_PRINTF
#else
#define FREEINK_LOG_TRANSPORT FREEINK_LOG_TRANSPORT_SERIAL
#endif
#endif

// Bidirectional serial transport exposed by the board's physical USB-C port.
// Most boards route it through Arduino's selected Serial implementation, while
// Sticky's on-board WCH bridge is wired to UART0 instead of native USB CDC.
#if FREEINK_DEVICE_STICKY
#define FREEINK_SERIAL_HAS_TX_TIMEOUT 0
#else
#define FREEINK_SERIAL_HAS_TX_TIMEOUT (ARDUINO_USB_CDC_ON_BOOT)
#endif

namespace BoardConfig {

#if FREEINK_DEVICE_STICKY
inline HardwareSerial& serialTransport() { return Serial0; }
#else
inline Print& serialTransport() { return Serial; }
#endif

// Physical device family. X3 and X4 are sibling devices on the same ESP32-C3
// board (identical pinout, different panel/size): both profiles compile into the
// C3 binary and one is chosen at runtime (setDisplayX3() -> selectDevice).
enum class Board : uint8_t {
  XteinkX4,
  XteinkX3,
  XteinkX3Uc8279,  // newer X3 production run: same board/glass, UC8279d controller
  XteinkX4Pro,  // ESP32-S3 sibling of the C3 X4: SSD1677 + GT911 touch + warm/cold frontlight
  M5StackPaperColor,
  MurphyM3,
  DeLink,
  LilyGoT5S3,
  LilyGoT5H716,
  M5PaperV11,
  Sticky,
};

// How the board reports button presses.
enum class InputStyle : uint8_t {
  XteinkAdcLadder,         // resistor ladder on two ADC pins (X3/X4)
  DigitalButtons,          // plain active-low GPIO buttons
  DigitalConfirmBackHold,  // confirm held > N ms synthesizes BACK (M5 PaperColor)
  DigitalConfirmPowerHold, // confirm click, power hold on a shared GPIO
  DigitalFiveKey,          // 3 physical GPIO keys + synthesized events (Murphy M3)
};

// Panel controller silicon. Drivers are selected from this at begin().
// LgfxEpd = a raw-parallel EPD with no on-glass controller, driven via LovyanGFX
// (e.g. ED047TC1 on LilyGo T5 S3).
// UC8179 and UC8279 are the UltraChip siblings that newer batches ship in place
// of the original controller (UC8179/UC8279 for the X4 family's SSD1677, UC8279d
// for the X3's UC8253). Same UC81xx KW command family, separate drivers. Which
// one a unit carries is resolved at boot by the display-bus probe (0x70 VER /
// 0x71 FLG read, which SSD1677 lacks; VER byte2 LUT_VER tells UC8179 from
// UC8279). NVS hw_calib/screenType is read for diagnostics only. See
// XteinkDetect::applyXteinkDisplayController.
enum class DisplayController : uint8_t { SSD1677, UC8253, ED2208, LgfxEpd, IT8951, UC8279, UC8179 };

// Optional capacitive touch controller.
enum class TouchController : uint8_t { None, Chsc6x, Gt911 };

// Optional audio output path. Murphy M3 ships an ES8388-compatible stereo
// codec (I2S slave, control over the shared touch I2C bus) — the contract was
// recovered from the OEM firmware dump; see the consumer's audio notes.
// M5 PaperColor ships an ES8311 mono codec + AW8737A speaker amp — the
// contract comes from the official pin map and M5Unified's speaker bring-up.
enum class AudioOutput : uint8_t { None, I2sDac, I2sEs8388, I2sEs8311, PwmBuzzer };

// Optional addressable RGB LED strip. PaperColor has two RGB LEDs on GPIO21
// behind the M5PM1 LDO3V3 RGB rail.
enum class LedColorOrder : uint8_t { RGB, GRB };

constexpr int8_t PIN_UNASSIGNED = -1;

struct DisplayPins {
  int8_t sclk;
  int8_t mosi;
  int8_t cs;
  int8_t dc;
  int8_t rst;
  int8_t busy;
  int8_t powerEnable;
};

struct SdPins {
  int8_t sclk;
  int8_t miso;
  int8_t mosi;
  int8_t cs;
  int8_t powerEnable;
  bool separateSpi;
  uint32_t spiHz;  // 0 = use the SD manager default (40 MHz)
  // Polarity of powerEnable. true (default) = active-high (drive HIGH to power the
  // card, LOW to cut it) as on most boards. false = active-LOW enable (e.g. X4 Pro's
  // GPIO5, which gates the card while held LOW); the sleep path must then drive it
  // HIGH to power the card down. Defaulted so existing initializers stay valid.
  bool powerActiveHigh = true;
};

// 4-bit SDMMC/SDIO wiring (e.g. de-link). SdFat can't drive SDIO, so a board with
// busWidth != 0 gets the native esp-idf SDMMC block device instead of SPI/SdFat.
struct SdmmcPins {
  int8_t clk;
  int8_t cmd;
  int8_t d0;
  int8_t d1;
  int8_t d2;
  int8_t d3;
  uint8_t busWidth;  // 0 = not an SDMMC board (use SdPins/SPI), 1 or 4 = SDMMC
};

// The I2C fuel-gauge silicon a board carries. Each type has its own register map and
// init, so BatteryMonitor dispatches on it. Bq27220: TI command registers, no profile
// upload (LilyGo/X3). Cw2017: CellWise gauge that needs an 80-byte BATINFO battery
// profile loaded before it reports a valid SoC (Xteink X4 Pro).
enum class GaugeType : uint8_t { Bq27220, Cw2017 };

// I2C fuel-gauge / charger wiring (e.g. BQ27220 + BQ25896 on LilyGo T5 S3). When
// gaugeAddr != 0 (and FREEINK_BATTERY_I2C_GAUGE is set), BatteryMonitor reads the
// gauge over I2C instead of an ADC pin. chargerAddr is optional (0 = none) and
// only used for charge status.
struct BatteryGaugeConfig {
  int8_t i2cSda;
  int8_t i2cScl;
  uint32_t i2cHz;
  uint8_t gaugeAddr;    // BQ27220 = 0x55; CW2017 = 0x63; 0 = no I2C gauge (use ADC)
  uint8_t chargerAddr;  // BQ25896 = 0x6B; 0 = none
  // Arduino I2C controller index: 0 = Wire, 1 = Wire1. Default 0. Set to 1 on
  // boards where the gauge sits on a different physical bus than another I2C
  // peripheral (e.g. Sticky's GT911 touch on Wire/SDA3-SCL2 vs gauge on
  // Wire1/SDA1-SCL0) so they don't fight over one controller. Only honored on
  // multi-bus SoCs (SOC_I2C_NUM > 1); single-bus parts (ESP32-C3) ignore it.
  uint8_t i2cBus = 0;
  GaugeType gaugeType = GaugeType::Bq27220;  // register map / init to use
};

struct InputPins {
  int8_t back;
  int8_t confirm;
  int8_t left;
  int8_t right;
  int8_t up;
  int8_t down;
  int8_t power;
  bool powerActiveHigh;  // true = pressed reads HIGH (INPUT_PULLDOWN); false = active-LOW (INPUT_PULLUP)
};

// Capacitive touch panel description (TouchController::None disables it).
struct TouchConfig {
  TouchController controller;
  int8_t sda;
  int8_t scl;
  int8_t irq;
  int8_t reset;
  uint8_t i2cAddress;
  uint16_t rawMinX, rawMaxX;  // raw controller range, mapped to display coords
  uint16_t rawMinY, rawMaxY;
  bool synthesizeConfirm;  // emit a CONFIRM button event on tap
  uint8_t i2cAddressAlt;   // alternate I2C address to probe (GT911 0x14; 0 = none)
  bool irqActiveLow;       // touch IRQ asserted LOW (CHSC6x)
  // GT911 point-frame layout: false = datasheet standard (track-id at 0x8150, so
  // coords start at byte 1); true = coords start at byte 0 (no track-id), as seen
  // on M5Paper's GT911 which boots without a reset/config dance. Ignored (CHSC6x).
  bool gt911CoordsAtByte0;
  // Touch power-rail enable. PIN_UNASSIGNED on boards whose touch controller is always
  // powered; otherwise driven to its ON level before the reset/probe on boards that
  // gate it (e.g. Sticky's active-high TOUCH_EN, or the X4 Pro's active-low GPIO2).
  // Default keeps existing initializers valid.
  int8_t powerEnable = PIN_UNASSIGNED;
  // Touch-to-panel mounting correction, applied to the raw coords so the touch
  // frame aligns with the display's NATIVE (panel) frame before orientation
  // mapping. swapXY first (digitizer rotated 90° vs panel, e.g. Sticky's portrait
  // sensor on a landscape panel), then per-axis flip. rawMinX/MaxX/etc describe the
  // POST-swap (panel) axes. Defaults = aligned. The display orientation is handled
  // separately by GfxRenderer::tapToLogical, so taps follow rotation automatically.
  bool swapXY = false;
  bool flipX = false;
  bool flipY = false;
  // Capacitive home key below the panel, reported by the touch controller itself
  // (GT911 "have key" status bit 0x10, surfaced as InputManager::wasHomeKeyPressed()).
  // Lets firmware move "exit to home" off a swipe gesture on boards that have one.
  bool hasHomeKey = false;
  // Polarity of powerEnable. true (default) = active-high (drive HIGH to power the
  // controller). false = active-LOW (drive LOW to power it, e.g. X4 Pro's GPIO2). The
  // reset path drives the ON level; the sleep path drives the OFF level.
  bool powerEnableActiveHigh = true;
};

// PWM frontlight description (gpio == PIN_UNASSIGNED disables it).
struct FrontlightConfig {
  int8_t gpio;  // primary channel: the sole LED on a single-channel board, or the "cool"
                // channel of a warm/cool pair.
  uint32_t pwmFrequency;
  uint8_t pwmResolutionBits;
  bool activeHigh;
  // Optional second PWM channel for a warm/cool color-temperature frontlight (e.g. the
  // Xteink X4 Pro: cool=gpio GPIO8, warm=gpioWarm GPIO9). PIN_UNASSIGNED on single-channel
  // boards (de-link / LilyGo / Murphy), where setColorTemperature() stays a no-op. The warm
  // channel shares the primary's frequency / resolution / active level. FrontlightManager
  // treats `gpio` as cool and `gpioWarm` as warm; if a board's pair is physically reversed,
  // the color-temperature direction inverts (cosmetic, and user-flippable in firmware).
  int8_t gpioWarm = PIN_UNASSIGNED;
};

// Audio output description (AudioOutput::None disables it).
struct AudioConfig {
  AudioOutput output;
  int8_t bclk;    // I2S bit clock (unused for PWM buzzer)
  int8_t lrclk;   // I2S word select (unused for PWM buzzer)
  int8_t dout;    // I2S data out, or the PWM pin for a buzzer
  int8_t mclk;    // I2S master clock (PIN_UNASSIGNED if not wired)
  int8_t enable;  // codec power / rail enable pin (PIN_UNASSIGNED if none)
  bool enableActiveHigh;
  int8_t ampEnable;  // separate speaker-amp enable (e.g. AW8737A SPK_EN), held
                     // high only while playing; PIN_UNASSIGNED if none. Active-high.
  int8_t codecSda;   // codec control I2C — may be a shared bus (e.g. touch)
  int8_t codecScl;
  uint8_t codecAddr;  // 7-bit codec address, 0 = no control codec
  int8_t buzzer;      // separate LEDC tone pin (PIN_UNASSIGNED if none)
};

struct LedConfig {
  int8_t data;
  uint8_t count;
  LedColorOrder colorOrder;
  bool pmicRgbPower;  // true = enable M5PM1 RGB LED power rail before use
};

// Microphone input path (MicInput::None disables it). PDM mics (e.g. the Sticky's
// MSM261DDB020) need a clock out + data in; `enable` powers the mic rail.
enum class MicInput : uint8_t { None, Pdm };
struct MicConfig {
  MicInput input;
  int8_t clk;     // PDM clock (output to mic)
  int8_t data;    // PDM data (input from mic)
  int8_t enable;  // mic power/enable pin (PIN_UNASSIGNED if none)
  bool enableActiveHigh;
};

enum class RtcType : uint8_t { None, Pcf8563, Ds3231 };
enum class ImuType : uint8_t { None, Lsm6ds3, Qmi8658 };

// On-board I2C sensors sharing one bus (e.g. the Sticky's RTC + temp/humidity +
// IMU on SDA1/SCL0, the same bus as its fuel gauge). Each addr is 0 when that
// sensor is absent; the matching sensor lib reads its addr from here.
struct SensorsConfig {
  int8_t i2cSda;
  int8_t i2cScl;
  uint32_t i2cHz;
  uint8_t rtcAddr;           // PCF8563 = 0x51, DS3231 = 0x68; 0 = none
  uint8_t tempHumidityAddr;  // SHT40 = 0x44; 0 = none
  uint8_t imuAddr;           // LSM6DS3TR-C = 0x6A, QMI8658 = 0x6B/0x6A; 0 = none
  uint8_t i2cBus;            // 0 = Wire, 1 = Wire1 on multi-bus SoCs
  RtcType rtcType;
  ImuType imuType;
};

// How the panel is mounted relative to the driver's native scan. Any board injects
// its own mirroring here; a 180° rotation is mirrorX && mirrorY. (90°/270° need a
// software transpose — they swap width/height and aren't expressible by panel RAM
// addressing alone — so they are not a flag here.)
struct DisplayOrientation {
  bool mirrorX;  // reverse source/column (X) order
  bool mirrorY;  // reverse gate/row (Y) order
};

// Power-rail latch pins a battery-powered board must drive HIGH early in boot
// to keep itself on (PWR_HOLD / PWR_LOCK style latches, e.g. the Sticky's
// GPIO45/46). Board truth lives here; asserting them is firmware policy — see
// holdPowerRails(). Releasing the pins later is a software power-off.
struct PowerConfig {
  int8_t latch0;
  int8_t latch1;
};

struct BoardProfile {
  Board board;
  const char* name;
  InputStyle inputStyle;
  DisplayController displayController;
  uint16_t displayWidth;
  uint16_t displayHeight;
  DisplayPins display;
  uint32_t displaySpiHz;  // 0 = use the panel driver's controller-appropriate default
  SdPins sd;
  InputPins input;
  int8_t batteryAdc;
  int8_t batteryChargeStatus;
  float batteryDividerMultiplier;
  int8_t usbDetect;
  TouchConfig touch;
  FrontlightConfig frontlight;
  AudioConfig audio;
  LedConfig leds;
  DisplayOrientation orientation;   // panel mount transform (mirrorX/mirrorY)
  SdmmcPins sdmmc;                  // 4-bit SDMMC wiring (busWidth 0 = use SPI/SdFat)
  BatteryGaugeConfig batteryGauge;  // I2C fuel gauge (gaugeAddr 0 = use ADC pin)
  // Microphone (PDM in). Defaulted so existing profiles need no change; a board
  // with a mic sets it. PIN_UNASSIGNED is -1 — do NOT rely on zero-init here.
  MicConfig mic = {MicInput::None, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, true};
  SensorsConfig sensors = {PIN_UNASSIGNED, PIN_UNASSIGNED, 0, 0, 0, 0, 0, RtcType::None, ImuType::None};
  float uiScale = 1.0f;
  PowerConfig power = {PIN_UNASSIGNED, PIN_UNASSIGNED};
  uint8_t displayControllerVariant = 0;
};

constexpr TouchConfig NO_TOUCH = {TouchController::None,
                                  PIN_UNASSIGNED,
                                  PIN_UNASSIGNED,
                                  PIN_UNASSIGNED,
                                  PIN_UNASSIGNED,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  false,
                                  0,
                                  false,
                                  false};

constexpr FrontlightConfig NO_FRONTLIGHT = {PIN_UNASSIGNED, 0, 0, true};
constexpr AudioConfig NO_AUDIO = {AudioOutput::None,
                                  PIN_UNASSIGNED,
                                  PIN_UNASSIGNED,
                                  PIN_UNASSIGNED,
                                  PIN_UNASSIGNED,
                                  PIN_UNASSIGNED,
                                  true,
                                  PIN_UNASSIGNED,
                                  PIN_UNASSIGNED,
                                  PIN_UNASSIGNED,
                                  0,
                                  PIN_UNASSIGNED};
constexpr LedConfig NO_LEDS = {PIN_UNASSIGNED, 0, LedColorOrder::GRB, false};

// Defaults matching the BoardProfile member initializers, so a profile can set a
// trailing field (e.g. uiScale) positionally without spelling out the literals.
constexpr MicConfig NO_MIC = {MicInput::None, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, true};
constexpr SensorsConfig NO_SENSORS = {PIN_UNASSIGNED, PIN_UNASSIGNED, 0, 0, 0, 0, 0, RtcType::None, ImuType::None};

constexpr DisplayOrientation NO_FLIP = {false, false};   // native scan
constexpr DisplayOrientation ROTATE_180 = {true, true};  // upside-down mount
constexpr DisplayOrientation MIRROR_X = {true, false};   // horizontal mirror
constexpr DisplayOrientation MIRROR_Y = {false, true};   // vertical mirror
constexpr SdmmcPins NO_SDMMC = {
    PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, 0};
constexpr BatteryGaugeConfig NO_GAUGE = {PIN_UNASSIGNED, PIN_UNASSIGNED, 0, 0, 0};  // ADC battery

#if FREEINK_DEVICE_X4
#include "boards/XteinkX4.h"
#endif
#if FREEINK_DEVICE_X3
#include "boards/XteinkX3.h"
#include "boards/XteinkX3Uc8279.h"
#endif
#if FREEINK_DEVICE_M5
#include "boards/M5StackPaperColor.h"
#endif
#if FREEINK_DEVICE_MURPHY
#include "boards/MurphyM3.h"
#endif
#if FREEINK_DEVICE_DELINK
#include "boards/DeLink.h"
#endif
#if FREEINK_DEVICE_LILYGO
#include "boards/LilygoT5S3.h"
#endif
#if FREEINK_DEVICE_LILYGO_H716
#include "boards/LilygoH716.h"
#endif
#if FREEINK_DEVICE_M5PAPER
#include "boards/M5PaperV11.h"
#endif
#if FREEINK_DEVICE_STICKY
#include "boards/Sticky.h"
#endif
#if FREEINK_DEVICE_X4PRO
#include "boards/XteinkX4Pro.h"
#endif

// Largest framebuffer (bytes) over the devices compiled into this build, derived
// from the profiles above. The display facade sizes its static framebuffer to
// this so one binary holds whichever panel is runtime-selected; a single-device
// build gets exactly that panel's size. Adding a device adds one term here — no
// device names leak into the display code.
constexpr uint32_t cmax(uint32_t a, uint32_t b) { return a > b ? a : b; }
constexpr uint32_t panelBytes(const BoardProfile& p) {
  return static_cast<uint32_t>(p.displayWidth / 8) * p.displayHeight;
}

#if FREEINK_DEVICE_X4
#define FB_X4 panelBytes(XTEINK_X4)
#else
#define FB_X4 0u
#endif
#if FREEINK_DEVICE_X3
#define FB_X3 panelBytes(XTEINK_X3)
#else
#define FB_X3 0u
#endif
#if FREEINK_DEVICE_M5
#define FB_M5 panelBytes(M5STACK_PAPER_COLOR)
#else
#define FB_M5 0u
#endif
#if FREEINK_DEVICE_MURPHY
#define FB_MURPHY panelBytes(MURPHY_M3)
#else
#define FB_MURPHY 0u
#endif
#if FREEINK_DEVICE_DELINK
#define FB_DELINK panelBytes(DE_LINK)
#else
#define FB_DELINK 0u
#endif
#if FREEINK_DEVICE_LILYGO
#define FB_LILYGO panelBytes(LILYGO_T5S3)
#elif FREEINK_DEVICE_LILYGO_H716
#define FB_LILYGO panelBytes(LILYGO_T5_H716)
#else
#define FB_LILYGO 0u
#endif
#if FREEINK_DEVICE_M5PAPER
#define FB_M5PAPER panelBytes(M5PAPER_V11)
#else
#define FB_M5PAPER 0u
#endif
#if FREEINK_DEVICE_X4PRO
#define FB_X4PRO panelBytes(XTEINK_X4_PRO)
#else
#define FB_X4PRO 0u
#endif
#if FREEINK_DEVICE_STICKY
#define FB_STICKY panelBytes(STICKY)
#else
#define FB_STICKY 0u
#endif

constexpr uint32_t MAX_FRAMEBUFFER_BYTES = cmax(FB_X4, cmax(FB_X3, cmax(FB_M5, cmax(FB_MURPHY, cmax(FB_DELINK, cmax(FB_LILYGO, cmax(FB_M5PAPER, cmax(FB_X4PRO, FB_STICKY))))))));

// Compile-time default device — the profile ACTIVE starts as. With a single
// device in the build this is the only device; with several same-MCU devices it
// is the boot default until the consumer calls selectDevice().
#if FREEINK_DEVICE_LILYGO_H716
constexpr BoardProfile DEFAULT_DEVICE = LILYGO_T5_H716;
#elif FREEINK_DEVICE_LILYGO
constexpr BoardProfile DEFAULT_DEVICE = LILYGO_T5S3;
#elif FREEINK_DEVICE_X4PRO
constexpr BoardProfile DEFAULT_DEVICE = XTEINK_X4_PRO;
#elif FREEINK_DEVICE_M5
constexpr BoardProfile DEFAULT_DEVICE = M5STACK_PAPER_COLOR;
#elif FREEINK_DEVICE_MURPHY
constexpr BoardProfile DEFAULT_DEVICE = MURPHY_M3;
#elif FREEINK_DEVICE_DELINK
constexpr BoardProfile DEFAULT_DEVICE = DE_LINK;
#elif FREEINK_DEVICE_M5PAPER
constexpr BoardProfile DEFAULT_DEVICE = M5PAPER_V11;
#elif FREEINK_DEVICE_STICKY
constexpr BoardProfile DEFAULT_DEVICE = STICKY;
#elif FREEINK_DEVICE_X3 && !FREEINK_DEVICE_X4
constexpr BoardProfile DEFAULT_DEVICE = XTEINK_X3;
#else
// X4-only or the dual X3+X4 C3 binary: boot as X4, runtime-swap to X3 on detect.
constexpr BoardProfile DEFAULT_DEVICE = XTEINK_X4;
#endif

// Runtime-active profile. Defaults to DEFAULT_DEVICE — identical to the old
// compile-time behavior when only one device is in the build. A consumer that
// ships multiple same-MCU devices in one binary calls selectDevice() after its
// own hardware detection, before any pin is used.
inline BoardProfile ACTIVE = DEFAULT_DEVICE;

inline void holdPowerRails();  // defined below; used by selectDevice()

// Set ACTIVE to one of the devices compiled into this build. Returns false (and
// leaves ACTIVE unchanged) if `which` was not included via -DFREEINK_DEVICE_*.
inline bool selectDevice(Board which) {
  switch (which) {
#if FREEINK_DEVICE_X4
    case Board::XteinkX4:
      ACTIVE = XTEINK_X4;
      break;
#endif
#if FREEINK_DEVICE_X3
    case Board::XteinkX3:
      ACTIVE = XTEINK_X3;
      break;
    case Board::XteinkX3Uc8279:
      ACTIVE = XTEINK_X3_UC8279;
      break;
#endif
#if FREEINK_DEVICE_M5
    case Board::M5StackPaperColor:
      ACTIVE = M5STACK_PAPER_COLOR;
      break;
#endif
#if FREEINK_DEVICE_MURPHY
    case Board::MurphyM3:
      ACTIVE = MURPHY_M3;
      break;
#endif
#if FREEINK_DEVICE_DELINK
    case Board::DeLink:
      ACTIVE = DE_LINK;
      break;
#endif
#if FREEINK_DEVICE_LILYGO
    case Board::LilyGoT5S3:
      ACTIVE = LILYGO_T5S3;
      break;
#endif
#if FREEINK_DEVICE_LILYGO_H716
    case Board::LilyGoT5H716:
      ACTIVE = LILYGO_T5_H716;
      break;
#endif
#if FREEINK_DEVICE_M5PAPER
    case Board::M5PaperV11:
      ACTIVE = M5PAPER_V11;
      break;
#endif
#if FREEINK_DEVICE_STICKY
    case Board::Sticky:
      ACTIVE = STICKY;
      break;
#endif
#if FREEINK_DEVICE_X4PRO
    case Board::XteinkX4Pro:
      ACTIVE = XTEINK_X4_PRO;
      break;
#endif
    default:
      return false;
  }
  // Runtime-selected boards resolve after the consumer's first-thing-in-setup()
  // holdPowerRails() call (the dual X3+X4 binary boots with the X4 profile and
  // detects the real board here), so re-assert the selected board's latch pins
  // now that they are known.
  holdPowerRails();
  return true;
}

inline bool isM5StackPaperColor() { return ACTIVE.board == Board::M5StackPaperColor; }
inline bool isMurphyM3() { return ACTIVE.board == Board::MurphyM3; }
inline bool isDeLink() { return ACTIVE.board == Board::DeLink; }
inline bool isM5PaperV11() { return ACTIVE.board == Board::M5PaperV11; }
inline bool isSticky() { return ACTIVE.board == Board::Sticky; }
inline bool isX4Pro() { return ACTIVE.board == Board::XteinkX4Pro; }
inline bool hasTouch() { return ACTIVE.touch.controller != TouchController::None; }
inline bool hasHomeKey() { return ACTIVE.touch.hasHomeKey; }
inline bool hasPwmFrontlight() { return ACTIVE.frontlight.gpio != PIN_UNASSIGNED; }
inline bool hasAudio() { return ACTIVE.audio.output != AudioOutput::None; }

// Safety guard: a power-latch pin must never coincide with a display or SDMMC
// bus pin. A latch is driven hard HIGH (asserted) or LOW (power-off) and held
// across sleep — if that pin is really, say, the display CS (GPIO13 on the X4
// Pro) or an SDMMC line, asserting the "latch" would clobber the bus. This
// catches a mis-set profile or an X4-vs-X4Pro config mixup before it drives the
// wrong pin. (The GPIO13 battery latch is correct on the C3 X4, where 13 is not
// a bus pin; on the X4 Pro 13 is the display CS, so a latch there is rejected.)
inline bool latchConflictsWithBus(int8_t pin) {
  if (pin < 0) return false;
  const DisplayPins& d = ACTIVE.display;
  if (pin == d.sclk || pin == d.mosi || pin == d.cs || pin == d.dc || pin == d.rst || pin == d.busy) return true;
  const SdmmcPins& s = ACTIVE.sdmmc;
  if (s.busWidth != 0 &&
      (pin == s.clk || pin == s.cmd || pin == s.d0 || pin == s.d1 || pin == s.d2 || pin == s.d3)) {
    return true;
  }
  return false;
}

// Assert the board's power-rail latch pins. Battery-latched boards (e.g. the
// Sticky) must call this first thing in setup() or the board powers off when
// the user releases the power button. Releasing the pins (driving them LOW)
// is a software power-off. No-op on boards without a latch.
inline void holdPowerRails() {
  for (const int8_t pin : {ACTIVE.power.latch0, ACTIVE.power.latch1}) {
    if (pin < 0) continue;
    if (latchConflictsWithBus(pin)) {
      // Refuse to drive a bus pin as a latch — see latchConflictsWithBus().
#if defined(ENABLE_SERIAL_LOG)
      // esp_rom_printf, not Serial: this runs first thing in setup(), before
      // USB CDC enumerates, and consumers deprecate Serial.printf in headers.
      esp_rom_printf("[BOARD] power latch pin %d collides with a display/SD bus pin; skipping\n", pin);
#endif
      continue;
    }
    // A previous power-off may have latched the pin LOW with gpio_hold_en —
    // a state that survives a reset and a USB-powered deep-sleep wake, and
    // silently defeats the digitalWrite below. Release it first.
    gpio_hold_dis(static_cast<gpio_num_t>(pin));
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
}

// Rescue the SD power rail before first display use. A previous firmware's
// sleep path may have latched the rail off with gpio_hold_en — a state that
// survives reset and reflashing — and on boards where SD shares the display's
// SPI bus an unpowered card clamps SCLK/MOSI so the panel never hears a
// command. Releases the hold, powers the card, and deselects its CS.
// SDCardManager::begin() does this itself; apps that skip SD should call this
// once before display.begin(). No-op on boards without a switched SD rail.
inline void releaseSdRail() {
  if (ACTIVE.sd.powerEnable >= 0) {
    gpio_hold_dis(static_cast<gpio_num_t>(ACTIVE.sd.powerEnable));
    pinMode(ACTIVE.sd.powerEnable, OUTPUT);
    // Drive the enable to its ON level: HIGH for active-high rails, LOW for the
    // active-low ones (X4 Pro's GPIO5 powers the card while held LOW).
    digitalWrite(ACTIVE.sd.powerEnable, ACTIVE.sd.powerActiveHigh ? HIGH : LOW);
  }
  if (ACTIVE.sd.cs >= 0) {
    pinMode(ACTIVE.sd.cs, OUTPUT);
    digitalWrite(ACTIVE.sd.cs, HIGH);
  }
}
inline bool hasMic() { return ACTIVE.mic.input != MicInput::None; }
inline bool hasBuzzer() { return ACTIVE.audio.buzzer != PIN_UNASSIGNED; }
inline bool hasRtc() { return ACTIVE.sensors.rtcAddr != 0; }
inline bool hasTempHumidity() { return ACTIVE.sensors.tempHumidityAddr != 0; }
inline bool hasImu() { return ACTIVE.sensors.imuAddr != 0; }
inline bool hasLeds() { return ACTIVE.leds.data != PIN_UNASSIGNED && ACTIVE.leds.count > 0; }

}  // namespace BoardConfig
