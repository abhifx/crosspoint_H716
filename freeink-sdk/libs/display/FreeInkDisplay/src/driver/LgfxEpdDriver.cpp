#include "LgfxEpdDriver.h"

#include <BoardConfig.h>

#include <cstring>

#if FREEINK_DRIVER_LGFX_EPD
#include <M5GFX.h>  // pulls LovyanGFX; added to lib_deps only on the LilyGo env
#include <esp_heap_caps.h>
#include <lgfx/v1/platforms/esp32/Bus_EPD.h>
#include <lgfx/v1/platforms/esp32/Panel_EPD.hpp>

#if FREEINK_DEVICE_LILYGO_H716
#include <BoardT5H716.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include <driver/gpio.h>
#endif
#endif

namespace freeink {

#if FREEINK_DRIVER_LGFX_EPD
namespace {

// Set from the active config in begin(), read by the bus subclass below. The
// driver is a singleton (one panel), so a file-scope pointer is fine and mirrors
// how M5GFX/LovyanGFX use global device objects.
const LgfxEpdPowerHooks* g_hooks = nullptr;

// Bus subclass that defers the board's power topology to injected hooks. Matches
// the two override points LovyanGFX exposes: init() (pin setup) and
// powerControl() (rail up/down), guarding the _pwr_on state itself.
class FreeInkBusEPD : public lgfx::Bus_EPD {
#if FREEINK_DEVICE_LILYGO_H716
 private:
  // Helper to push to SR inside an ISR (MSB First for H716)
  static void IRAM_ATTR push_sr_isr(uint8_t data) {
    // Pull STR (GPIO0) LOW during shift to prevent output glitches
    lgfx::gpio_lo(H716_SR_STR);
    for (int i = 7; i >= 0; i--) {
      if (data & (1 << i)) lgfx::gpio_hi(H716_SR_DATA);
      else lgfx::gpio_lo(H716_SR_DATA);
      lgfx::gpio_lo(H716_SR_CLK);
      asm volatile("nop; nop; nop; nop; nop; nop; nop; nop;");
      lgfx::gpio_hi(H716_SR_CLK);
      asm volatile("nop; nop; nop; nop; nop; nop; nop; nop;");
    }
    // Pulse STR HIGH and leave HIGH so BOOT strap pin is satisfied on reset
    lgfx::gpio_hi(H716_SR_STR);
    ets_delay_us(1);
  }

  static bool IRAM_ATTR h716_line_done_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    auto me = (FreeInkBusEPD*)user_ctx;

    // 1. Pulse LE (Latch Enable) - Bit 0 of SR
    push_sr_isr(BoardT5H716::H716_SR_BASE | BoardT5H716::SR_BIT_LE); // LE High
    ets_delay_us(10);
    push_sr_isr(BoardT5H716::H716_SR_BASE);                       // LE Low

    // 2. Pulse CKV (Clock Vertical) - GPIO 38
    lgfx::gpio_lo(EP_CKV);
    ets_delay_us(10);
    lgfx::gpio_hi(EP_CKV);

    // 3. Clear busy flag
    struct BusEPD_Hack : public lgfx::Bus_EPD {
      void setBusy(bool b) { _bus_busy = b; }
    };
    ((BusEPD_Hack*)me)->setBusy(false);

    return false;
  }
#endif

 public:
  bool init() override {
#if FREEINK_DEVICE_LILYGO_H716
    _bus_busy = false;
    _pwr_on = false;

    lgfx::pinMode(EP_CKV, lgfx::pin_mode_t::output); // CKV
    lgfx::pinMode(_config.pin_sph, lgfx::pin_mode_t::output); // STH
    lgfx::pinMode(_config.pin_cl, lgfx::pin_mode_t::output); // CKH

    esp_lcd_i80_bus_config_t bus_config;
    memset(&bus_config, 0, sizeof(esp_lcd_i80_bus_config_t));
    bus_config.max_transfer_bytes = 4096;
    bus_config.clk_src = LCD_CLK_SRC_PLL160M;
    bus_config.dc_gpio_num = (gpio_num_t)_config.pin_pwr;
    bus_config.wr_gpio_num = (gpio_num_t)_config.pin_cl;
    bus_config.bus_width = _config.bus_width;
    for (int i = 0; i < _config.bus_width; ++i) {
      bus_config.data_gpio_nums[i] = (gpio_num_t)_config.pin_data[i];
      lgfx::pinMode(_config.pin_data[i], lgfx::pin_mode_t::output);
    }
    if (ESP_OK != esp_lcd_new_i80_bus(&bus_config, &_i80_bus_handle)) return false;

    esp_lcd_panel_io_i80_config_t io_config;
    memset(&io_config, 0, sizeof(esp_lcd_panel_io_i80_config_t));
    io_config.trans_queue_depth = 4;
    io_config.on_color_trans_done = h716_line_done_callback;
    io_config.user_ctx = this;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.pclk_hz = _config.bus_speed;
    io_config.flags.pclk_idle_low = 1;
    io_config.cs_gpio_num = (gpio_num_t)_config.pin_sph;

    if (ESP_OK != esp_lcd_new_panel_io_i80(_i80_bus_handle, &io_config, &_io_handle)) return false;

    return true;
#else
    if (g_hooks && g_hooks->prepare && !g_hooks->prepare()) return false;
    return lgfx::Bus_EPD::init();
#endif
  }

#if FREEINK_DEVICE_LILYGO_H716
  void beginTransaction(void) override {
    while (_bus_busy) { taskYIELD(); }

    // Start Frame: Pulse STV (Bit 4) LOW then HIGH
    BoardT5H716::pushShiftRegister(BoardT5H716::H716_SR_BASE & ~BoardT5H716::SR_BIT_STV); // STV Low
    lgfx::delayMicroseconds(10);
    lgfx::gpio_lo(EP_CKV); // CKV
    lgfx::delayMicroseconds(10);
    lgfx::gpio_hi(EP_CKV);
    BoardT5H716::pushShiftRegister(BoardT5H716::H716_SR_BASE); // STV High (Inactive)
    lgfx::delayMicroseconds(50);

    for (int i = 0; i < 3; ++i) {
      lgfx::gpio_lo(EP_CKV);
      lgfx::delayMicroseconds(10);
      lgfx::gpio_hi(EP_CKV);
    }
  }

  void endTransaction(void) override {
    while (_bus_busy) { taskYIELD(); }
  }
#endif

  bool powerControl(const bool powerOn) override {
    if (_pwr_on == powerOn) return true;
    wait();
    if (powerOn) {
      if (g_hooks && g_hooks->powerOn && !g_hooks->powerOn()) return false;
      _pwr_on = true;
      return true;
    }
    if (g_hooks && g_hooks->powerOff) g_hooks->powerOff();
    _pwr_on = false;
    return true;
  }
};

class FreeInkLgfxEpd : public lgfx::LGFX_Device {
 public:
  void setup(const LgfxEpdConfig& c, uint16_t w, uint16_t h) {
    auto bc = _bus.config();
    bc.bus_speed = c.busHz;
    for (int i = 0; i < 8; ++i) bc.pin_data[i] = c.dataPins[i];
    bc.pin_pwr = c.pinPwr;
    bc.pin_sph = c.pinSph;
    bc.pin_spv = c.pinSpv;
    bc.pin_oe = c.pinOe;
    bc.pin_le = c.pinLe;
    bc.pin_cl = c.pinCl;
    bc.pin_ckv = c.pinCkv;
    bc.bus_width = 8;
    _bus.config(bc);

    _panel.setBus(&_bus);

    auto dc = _panel.config_detail();
    dc.line_padding = c.linePadding;
    if (c.lutQuality && c.lutQualityStep) {
      dc.lut_quality = c.lutQuality;
      dc.lut_quality_step = c.lutQualityStep;
    }
    if (c.lutText && c.lutTextStep) {
      dc.lut_text = c.lutText;
      dc.lut_text_step = c.lutTextStep;
    }
    if (c.lutFast && c.lutFastStep) {
      dc.lut_fast = c.lutFast;
      dc.lut_fast_step = c.lutFastStep;
    }
    if (c.lutFastest && c.lutFastestStep) {
      dc.lut_fastest = c.lutFastest;
      dc.lut_fastest_step = c.lutFastestStep;
    }
    _panel.config_detail(dc);

    auto pc = _panel.config();
    pc.memory_width = pc.panel_width = w;
    pc.memory_height = pc.panel_height = h;
    pc.offset_rotation = 0;
    pc.offset_x = 0;
    pc.offset_y = 0;
    pc.bus_shared = false;
    _panel.config(pc);

    setPanel(&_panel);
  }

 private:
  FreeInkBusEPD _bus;
  lgfx::Panel_EPD _panel;
};

FreeInkLgfxEpd g_dev;

lgfx::epd_mode::epd_mode_t epdModeFor(RefreshMode m) {
  switch (m) {
    case RefreshMode::Full: return lgfx::epd_mode::epd_text;
    case RefreshMode::Half: return lgfx::epd_mode::epd_text;
    default: return lgfx::epd_mode::epd_fast;
  }
}

// 8-bit gray canvas (PSRAM) the panel pushes from, plus the two 1-bpp planes the
// facade streams for grayscale. Allocated once in begin().
lgfx::LGFX_Sprite* g_canvas = nullptr;
uint8_t* g_lsb = nullptr;
uint8_t* g_msb = nullptr;
uint16_t g_w = 0, g_h = 0, g_wb = 0;

constexpr uint8_t kGrayBlack = 0x00, kGrayDark = 0x55, kGrayLight = 0xAA, kGrayWhite = 0xFF;

// (base, lsb, msb) per pixel -> 4 gray levels, matching the reference port: a set
// base bit is white; otherwise msb/lsb pick light/dark; clear is black.
inline uint8_t grayValue(uint8_t base, uint8_t lsb, uint8_t msb, uint8_t mask) {
  if (base & mask) return kGrayWhite;
  const bool l = lsb & mask, m = msb & mask;
  if (m && l) return kGrayDark;
  if (m) return kGrayLight;
  if (l) return kGrayDark;
  return kGrayBlack;
}

void allocCanvas(uint16_t w, uint16_t h) {
  g_w = w;
  g_h = h;
  g_wb = w / 8;
  if (!g_canvas) {
    g_canvas = new lgfx::LGFX_Sprite(&g_dev);
    g_canvas->setPsram(true);
    g_canvas->setColorDepth(lgfx::color_depth_t::grayscale_8bit);
    g_canvas->createSprite(w, h);
  }
  const size_t planeBytes = static_cast<size_t>(g_wb) * h;
  if (!g_lsb) g_lsb = static_cast<uint8_t*>(heap_caps_malloc(planeBytes, MALLOC_CAP_SPIRAM));
  if (!g_msb) g_msb = static_cast<uint8_t*>(heap_caps_malloc(planeBytes, MALLOC_CAP_SPIRAM));
}

// Expand a 1-bpp B/W frame (bit set = white) into the 8-bit gray canvas.
void fillCanvasBW(const uint8_t* fb) {
  if (!g_canvas) return;
  auto* dst = static_cast<uint8_t*>(g_canvas->getBuffer());
  if (!dst) return;
  for (uint16_t y = 0; y < g_h; ++y) {
    const uint8_t* src = fb + static_cast<uint32_t>(y) * g_wb;
    uint8_t* drow = dst + static_cast<uint32_t>(y) * g_w;
    for (uint16_t bx = 0; bx < g_wb; ++bx) {
      const uint8_t b = src[bx];
      for (uint8_t bit = 0; bit < 8; ++bit) drow[bx * 8 + bit] = (b & (0x80 >> bit)) ? kGrayWhite : kGrayBlack;
    }
  }
}

// Combine the B/W base + buffered LSB/MSB planes into the 8-bit gray canvas.
void fillCanvasGray(const uint8_t* base) {
  if (!g_canvas || !g_lsb || !g_msb) return;
  auto* dst = static_cast<uint8_t*>(g_canvas->getBuffer());
  if (!dst) return;
  for (uint16_t y = 0; y < g_h; ++y) {
    const uint8_t* brow = base + static_cast<uint32_t>(y) * g_wb;
    const uint8_t* lrow = g_lsb + static_cast<uint32_t>(y) * g_wb;
    const uint8_t* mrow = g_msb + static_cast<uint32_t>(y) * g_wb;
    uint8_t* drow = dst + static_cast<uint32_t>(y) * g_w;
    for (uint16_t bx = 0; bx < g_wb; ++bx) {
      for (uint8_t bit = 0; bit < 8; ++bit) {
        const uint8_t mask = 0x80 >> bit;
        drow[bx * 8 + bit] = grayValue(brow[bx], lrow[bx], mrow[bx], mask);
      }
    }
  }
}

void pushCanvas(lgfx::epd_mode::epd_mode_t epdMode) {
  if (!g_canvas) return;
  g_dev.waitDisplay();
  g_dev.setEpdMode(epdMode);
  g_canvas->pushSprite(0, 0);  // commits to the panel; Panel_EPD runs the refresh
  g_dev.waitDisplay();
}

}  // namespace
#endif  // FREEINK_DRIVER_LGFX_EPD

LgfxEpdDriver::LgfxEpdDriver(const LgfxEpdConfig& cfg) : _cfg(cfg) {}

PanelGeometry LgfxEpdDriver::geometry() const {
  const uint16_t w = BoardConfig::ACTIVE.displayWidth;
  const uint16_t h = BoardConfig::ACTIVE.displayHeight;
  const uint16_t wb = w / 8;
  return {w, h, wb, static_cast<uint32_t>(wb) * h};
}

void LgfxEpdDriver::begin(EpdBus& bus) {
  (void)bus;
#if FREEINK_DRIVER_LGFX_EPD
  g_hooks = &_cfg.power;
  g_dev.setup(_cfg, BoardConfig::ACTIVE.displayWidth, BoardConfig::ACTIVE.displayHeight);
  g_dev.init();
  g_dev.setRotation(_cfg.rotation);
  g_dev.setEpdMode(lgfx::epd_mode::epd_fast);
  allocCanvas(BoardConfig::ACTIVE.displayWidth, BoardConfig::ACTIVE.displayHeight);
#endif
}

void LgfxEpdDriver::display(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, RefreshMode mode, bool turnOff) {
  (void)bus;
  (void)prev;
#if FREEINK_DRIVER_LGFX_EPD
  fillCanvasBW(fb);          // expand the 1-bpp frame into the gray canvas
  pushCanvas(epdModeFor(mode));
  if (turnOff) g_dev.sleep();
#else
  (void)fb;
  (void)mode;
  (void)turnOff;
#endif
}

void LgfxEpdDriver::copyGrayscaleLsb(EpdBus& bus, const uint8_t* lsb) {
  (void)bus;
#if FREEINK_DRIVER_LGFX_EPD
  if (g_lsb && lsb) memcpy(g_lsb, lsb, static_cast<size_t>(g_wb) * g_h);
#else
  (void)lsb;
#endif
}

void LgfxEpdDriver::copyGrayscaleMsb(EpdBus& bus, const uint8_t* msb) {
  (void)bus;
#if FREEINK_DRIVER_LGFX_EPD
  if (g_msb && msb) memcpy(g_msb, msb, static_cast<size_t>(g_wb) * g_h);
#else
  (void)msb;
#endif
}

void LgfxEpdDriver::writeGrayscalePlaneStrip(EpdBus& bus, GrayPlane plane, const uint8_t* rows, uint16_t yStart,
                                             uint16_t numRows) {
  (void)bus;
#if FREEINK_DRIVER_LGFX_EPD
  uint8_t* dstPlane = (plane == GrayPlane::Lsb) ? g_lsb : g_msb;
  if (!dstPlane || !rows) return;
  const uint32_t offset = static_cast<uint32_t>(yStart) * g_wb;
  memcpy(dstPlane + offset, rows, static_cast<size_t>(numRows) * g_wb);
#else
  (void)plane;
  (void)rows;
  (void)yStart;
  (void)numRows;
#endif
}

void LgfxEpdDriver::displayGray(EpdBus& bus, const uint8_t* fb, bool turnOff, const unsigned char* lut,
                                bool factoryMode) {
  (void)bus;
  (void)lut;
  (void)factoryMode;
#if FREEINK_DRIVER_LGFX_EPD
  fillCanvasGray(fb);  // combine base + LSB/MSB planes -> 4-level gray
  // Same mode as the B/W base push: Panel_EPD's per-pixel diff keys on the
  // epd_mode LUT offset, so switching modes here would re-drive every pixel
  // (full-screen inversion flash). The board's fast LUT carries both the B/W
  // drive and the AA gray-nudge columns, so one mode serves both pushes.
  pushCanvas(lgfx::epd_mode::epd_fast);
  if (turnOff) g_dev.sleep();
#else
  (void)fb;
  (void)turnOff;
#endif
}

void LgfxEpdDriver::cleanupGrayscaleBuffers(EpdBus& bus, const uint8_t* bw) {
  (void)bus;
#if FREEINK_DRIVER_LGFX_EPD
  if (!bw) return;
  fillCanvasBW(bw);
#else
  (void)bw;
#endif
}

void LgfxEpdDriver::deepSleep(EpdBus& bus) {
  (void)bus;
#if FREEINK_DRIVER_LGFX_EPD
  g_dev.sleep();
#endif
}

// Per-board config injection. This driver has NO universal default — the bus pins
// and power hooks are entirely board-specific — so a LilyGo-class board defines
// `const LgfxEpdConfig& yourConfig();` in namespace freeink and builds with
// -DFREEINK_LGFX_EPD_CONFIG=yourConfig. The SDK's LilyGo board-support library
// provides the default config for FREEINK_DEVICE_LILYGO builds.
#if FREEINK_DEVICE_LILYGO
const LgfxEpdConfig& lilygoT5S3LgfxConfig();
PanelDriver& lgfxEpdDriver() {
  static LgfxEpdDriver instance(lilygoT5S3LgfxConfig());
  return instance;
}
#elif FREEINK_DEVICE_LILYGO_H716
const LgfxEpdConfig& lilygoT5H716LgfxConfig();
PanelDriver& lgfxEpdDriver() {
  static LgfxEpdDriver instance(lilygoT5H716LgfxConfig());
  return instance;
}
#elif defined(FREEINK_LGFX_EPD_CONFIG)
const LgfxEpdConfig& FREEINK_LGFX_EPD_CONFIG();
PanelDriver& lgfxEpdDriver() {
  static LgfxEpdDriver instance(FREEINK_LGFX_EPD_CONFIG());
  return instance;
}
#elif FREEINK_DRIVER_LGFX_EPD
#error "FREEINK_DRIVER_LGFX_EPD requires a board config: define `const LgfxEpdConfig& yourConfig();` in namespace freeink and build with -DFREEINK_LGFX_EPD_CONFIG=yourConfig"
#else
// Driver not selected in this build: provide a stub so the accessor still links if
// referenced. Never called (the facade only selects it under FREEINK_DRIVER_LGFX_EPD).
PanelDriver& lgfxEpdDriver() {
  static const LgfxEpdConfig kNone = {};
  static LgfxEpdDriver instance(kNone);
  return instance;
}
#endif

}  // namespace freeink
