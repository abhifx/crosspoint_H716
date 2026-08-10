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
    LOG_ERR("PDR", "setGreyLevels(16) FAILED! Falling back to 4 levels.");
  }

  _painter.setQuality(EPD_Painter::Quality::QUALITY_HIGH); // High quality is required for photographic detail

  auto& cfg = EPD_Painter_Access::getMutableConfig(_painter);
  // Reverting to default timings which are known to work in Tony's demos
  cfg.g16_pass_us_normal = 15000;
  cfg.g16_pass_us_high   = 15000;

  size_t planeBytes = static_cast<size_t>(_wb) * _h;
  if (!_lsb) _lsb = (uint8_t*)heap_caps_malloc(planeBytes, MALLOC_CAP_SPIRAM);
  if (!_msb) _msb = (uint8_t*)heap_caps_malloc(planeBytes, MALLOC_CAP_SPIRAM);
  memset(_lsb, 0x00, planeBytes);
  memset(_msb, 0x00, planeBytes);

  if (!_painterFb) {
     _painterFb = (uint8_t*)heap_caps_aligned_alloc(16, static_cast<size_t>(_w) * _h, MALLOC_CAP_SPIRAM);
     if (_painterFb) memset(_painterFb, 0x00, static_cast<size_t>(_w) * _h); // Start White (0)
  }

  // Mandatory hardware clear on startup to sync state
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
    uint8_t* drow = _painterFb + (static_cast<size_t>(y) * _w);

    for (uint16_t bx = 0; bx < _wb; bx++) {
      uint8_t b = brow[bx];

      for (int bit = 0; bit < 8; bit++) {
        uint8_t mask = 0x80 >> bit;
        // CrossPoint 1-bit: 1=White, 0=Black.
        // hardware 0=White, 15=Black.
        drow[bx * 8 + bit] = (b & mask) ? 0 : 15;
      }
    }
  }
}

void PainterDriver::syncToPainter8Bit(const uint8_t* grayBuf) {
  if (!_painterFb || !grayBuf) return;

  // Optimized Direct Mapping using measured hardware luminance levels.
  // This replaces linear bit-shifting and manual contrast curves with
  // accurate physical shade matching, eliminating banding.
  // grayBuf: 0=Black, 255=White.
  // H716_LEVEL_LUM: index=HardwareLevel (0=White, 15=Black), value=PhysicalLuminance (255=White, 0=Black).

  for (uint32_t i = 0; i < (uint32_t)_w * _h; i++) {
    uint8_t targetLum = grayBuf[i];

    // Find hardware level whose luminance is closest to targetLum.
    // Since H716_LEVEL_LUM is strictly monotonic (255 down to 0),
    // we can use a fast search.
    uint8_t bestLevel = 0;
    int minDiff = 256;
    for (uint8_t level = 0; level < 16; level++) {
      int diff = abs((int)targetLum - (int)H716_LEVEL_LUM[level]);
      if (diff < minDiff) {
        minDiff = diff;
        bestLevel = level;
      } else if (diff > minDiff) {
        // Since it's monotonic, as soon as diff starts increasing, we've passed the best match.
        break;
      }
    }
    _painterFb[i] = bestLevel;
  }
}

void PainterDriver::display(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, RefreshMode mode, bool turnOff) {
  // Sync 1-bit UI into the 16-level hardware buffer.
  // This uses our new non-inverted mapping logic.
  syncToPainter(fb, nullptr, nullptr);

  if (mode == RefreshMode::Full) {
    _painter.clear();
  }

  // Paint to hardware
  _painter.paint(_painterFb);

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
}

void PainterDriver::displayGray8Bit(EpdBus& bus, const uint8_t* grayBuf, RefreshMode mode, bool turnOff) {
  syncToPainter8Bit(grayBuf);

  if (mode == RefreshMode::Full) {
    LOG_INF("PDR", "Grayscale full refresh: clearing...");
    _painter.clear();
  }

  _painter.paint(_painterFb);
  // Double paint ensures levels settle properly on H716 hardware
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
