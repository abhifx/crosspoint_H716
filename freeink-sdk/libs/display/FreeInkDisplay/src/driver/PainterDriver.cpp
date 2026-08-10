#include "PainterDriver.h"
#include <BoardConfig.h>
#include <Logging.h>
#include <EPD_Painter_presets.h>
#include <math.h>

namespace freeink {

// Access internal config
struct EPD_Painter_Access : public EPD_Painter {
  static Config& getMutableConfig(EPD_Painter& p) {
    return p._config;
  }
};

PainterDriver::PainterDriver() : _painter(EPD_LILYGO_EPD47_H716_PRESET) {
  _w = 960;
  _h = 540;
  _wb = _w / 8;
}

PainterDriver::~PainterDriver() {
  if (_lsb) free(_lsb);
  if (_msb) free(_msb);
  if (_painterFb) heap_caps_free(_painterFb);
}

PanelGeometry PainterDriver::geometry() const {
  return {_w, _h, static_cast<uint16_t>(_wb), static_cast<uint32_t>(_wb) * _h};
}

// Measured luminance levels for H716 QUALITY_HIGH (0=White, 15=Black)
static const uint8_t H716_LEVEL_LUM[16] = { 255, 218, 207, 194, 185, 160, 150, 141, 116, 82, 75, 56, 52, 32, 11, 0 };
static uint8_t gamma_lut[256];

// Bayer 8x8 matrix for smoother, non-grainy dithering
static const uint8_t bayer8x8[8][8] = {
    { 0, 32,  8, 40,  2, 34, 10, 42},
    {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44,  4, 36, 14, 46,  6, 38},
    {60, 28, 68, 20, 62, 30, 70, 22},
    { 3, 35, 11, 43,  1, 33,  9, 41},
    {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47,  7, 39, 13, 45,  5, 37},
    {63, 31, 71, 23, 61, 29, 69, 21}
};

void PainterDriver::begin(EpdBus& bus) {
  (void)bus;
  LOG_INF("PDR", "Initializing EPD_Painter...");
  _painter.setAutoShutdown(false);
  if (!_painter.begin()) {
    LOG_ERR("PDR", "EPD_Painter init failed!");
    return;
  }

  _painter.setGreyLevels(16);
  _painter.setQuality(EPD_Painter::Quality::QUALITY_HIGH);

  auto& cfg = EPD_Painter_Access::getMutableConfig(_painter);
  cfg.g16_pass_us_normal = 20000;
  cfg.g16_pass_us_high   = 20000;

  size_t planeBytes = static_cast<size_t>(_wb) * _h;
  if (!_lsb) _lsb = (uint8_t*)malloc(planeBytes);
  if (!_msb) _msb = (uint8_t*)malloc(planeBytes);
  memset(_lsb, 0x00, planeBytes);
  memset(_msb, 0x00, planeBytes);

  if (!_painterFb) {
     _painterFb = (uint8_t*)heap_caps_aligned_alloc(16, static_cast<size_t>(_w) * _h, MALLOC_CAP_SPIRAM);
     if (_painterFb) memset(_painterFb, 0x00, static_cast<size_t>(_w) * _h); // White (0)
  }

  // Build Gamma LUT: 2.2 lift for photographic midtones (Kindle-style)
  for (int i = 0; i < 256; i++) {
    gamma_lut[i] = (uint8_t)(pow(i / 255.0, 1.0 / 2.2) * 255.0);
  }

  _painter.clear();
  LOG_INF("PDR", "PainterDriver ready.");
}

void PainterDriver::deepSleep(EpdBus& bus) {
  (void)bus;
  _painter.shutdown();
}

void PainterDriver::syncToPainter(const uint8_t* bw, const uint8_t* lsb, const uint8_t* msb) {
  if (!_painterFb) return;

  for (uint32_t y = 0; y < _h; y++) {
    const uint8_t* brow = bw + y * _wb;
    const uint8_t* lrow = lsb ? (lsb + y * _wb) : nullptr;
    const uint8_t* mrow = msb ? (msb + y * _wb) : nullptr;
    uint8_t* drow = _painterFb + (static_cast<size_t>(y) * _w);

    for (uint16_t bx = 0; bx < _wb; bx++) {
      uint8_t b = brow[bx];
      uint8_t l = lrow ? lrow[bx] : 0x00;
      uint8_t m = mrow ? mrow[bx] : 0x00;

      for (int bit = 0; bit < 8; bit++) {
        uint8_t mask = 0x80 >> bit;
        uint8_t level = 0; // White (0)

        if (!(b & mask)) {
          bool mm = (m & mask);
          bool ll = (l & mask);
          if (mm && ll) level = 10;
          else if (mm && !ll) level = 5;
          else level = 15;
        }
        drow[bx * 8 + bit] = level;
      }
    }
  }
}

void PainterDriver::syncToPainter8Bit(const uint8_t* grayBuf) {
  if (!_painterFb || !grayBuf) return;

  for (uint32_t y = 0; y < _h; y++) {
    const uint8_t* src_row = grayBuf + (static_cast<size_t>(y) * _w);
    uint8_t* dst_row = _painterFb + (static_cast<size_t>(y) * _w);

    for (uint32_t x = 0; x < _w; x++) {
      // 1. Apply Gamma
      uint8_t raw = src_row[x];
      uint8_t gray = gamma_lut[raw];

      // 2. Ordered Dithering (Bayer 8x8) for smoothness
      // Matrix value is 0..63. Map to +/- 8 lum range.
      int dither = (int)(bayer8x8[y & 7][x & 7]) - 32;
      int pixel = (int)gray + (dither / 4);
      if (pixel < 0) pixel = 0;
      if (pixel > 255) pixel = 255;

      // 3. Quantize to nearest hardware luminance level
      uint8_t best_level = 0;
      int min_diff = 1000;
      for (int l = 0; l < 16; l++) {
        int diff = abs(pixel - H716_LEVEL_LUM[l]);
        if (diff < min_diff) {
          min_diff = diff;
          best_level = (uint8_t)l;
        }
      }
      dst_row[x] = best_level;
    }
  }
}

void PainterDriver::display(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, RefreshMode mode, bool turnOff) {
  // Redirect to the 8-bit path for H716 consistency.
  syncToPainter(fb, nullptr, nullptr);

  if (mode == RefreshMode::Full) {
    LOG_INF("PDR", "Full refresh requested, clearing...");
    _painter.clear();
  }

  _painter.paint(_painterFb);
  _painter.paint(_painterFb); // Double paint for stability on H716

  if (turnOff) {
    _painter.shutdown();
  }
}

void PainterDriver::copyGrayscaleLsb(EpdBus& bus, const uint8_t* lsb) {
  if (lsb && _lsb) memcpy(_lsb, lsb, static_cast<size_t>(_wb) * _h);
}

void PainterDriver::copyGrayscaleMsb(EpdBus& bus, const uint8_t* msb) {
  if (msb && _msb) memcpy(_msb, msb, static_cast<size_t>(_wb) * _h);
}

void PainterDriver::displayGray(EpdBus& bus, const uint8_t* fb, bool turnOff, const unsigned char* lut, bool factoryMode) {
  syncToPainter(fb, _lsb, _msb);
  _painter.paint(_painterFb);
  vTaskDelay(pdMS_TO_TICKS(10));
  _painter.paint(_painterFb); // Double paint for ghosting cleanup
}

void PainterDriver::displayGray8Bit(EpdBus& bus, const uint8_t* grayBuf, RefreshMode mode, bool turnOff) {
  syncToPainter8Bit(grayBuf);

  if (mode == RefreshMode::Full) {
    LOG_INF("PDR", "Grayscale full refresh: clearing...");
    _painter.clear();
  }

  _painter.paint(_painterFb);
  // H716 hardware requires a settling pass for full 16-level fidelity.
  _painter.paint(_painterFb);

  if (turnOff) {
    _painter.shutdown();
  }
}

void PainterDriver::cleanupGrayscaleBuffers(EpdBus& bus, const uint8_t* bw) {
}

PanelDriver& painterDriver() {
  static PainterDriver instance;
  return instance;
}

} // namespace freeink
