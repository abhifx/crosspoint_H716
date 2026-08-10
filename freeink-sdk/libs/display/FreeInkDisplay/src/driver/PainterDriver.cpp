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

void PainterDriver::begin(EpdBus& bus) {
  (void)bus;
  LOG_INF("PDR", "Initializing EPD_Painter...");
  _painter.setAutoShutdown(false);
  if (!_painter.begin()) {
    LOG_ERR("PDR", "EPD_Painter init failed!");
    return;
  }

  LOG_INF("PDR", "Setting 16 gray levels...");
  if (!_painter.setGreyLevels(16)) {
    LOG_ERR("PDR", "setGreyLevels(16) FAILED!");
  }

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
     if (_painterFb) memset(_painterFb, 0x00, static_cast<size_t>(_w) * _h); // Start White (0)
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
          // Pixel is inked (b=0).
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

  // Accurate Physical Mapping: No Dithering.
  for (uint32_t i = 0; i < (uint32_t)_w * _h; i++) {
    uint8_t targetLum = grayBuf[i];
    uint8_t bestLevel = 0;
    int minDiff = 256;
    for (uint8_t level = 0; level < 16; level++) {
      int diff = abs((int)targetLum - (int)H716_LEVEL_LUM[level]);
      if (diff < minDiff) {
        minDiff = diff;
        bestLevel = level;
      } else break;
    }
    _painterFb[i] = bestLevel;
  }
}

void PainterDriver::display(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, RefreshMode mode, bool turnOff) {
  syncToPainter(fb, nullptr, nullptr);
  if (mode == RefreshMode::Full) _painter.clear();
  _painter.paint(_painterFb);
  _painter.paint(_painterFb);
  if (turnOff) _painter.shutdown();
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
  _painter.paint(_painterFb);
}

void PainterDriver::displayGray8Bit(EpdBus& bus, const uint8_t* grayBuf, RefreshMode mode, bool turnOff) {
  syncToPainter8Bit(grayBuf);
  if (mode == RefreshMode::Full) _painter.clear();
  _painter.paint(_painterFb);
  _painter.paint(_painterFb);
  if (turnOff) _painter.shutdown();
}

void PainterDriver::cleanupGrayscaleBuffers(EpdBus& bus, const uint8_t* bw) {
}

PanelDriver& painterDriver() {
  static PainterDriver instance;
  return instance;
}

} // namespace freeink
