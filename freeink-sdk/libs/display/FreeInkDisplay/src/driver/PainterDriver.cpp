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
  delay(100); // Give PMIC extra time to stabilize rails

  LOG_INF("PDR", "Setting 16 gray levels...");
  if (!_painter.setGreyLevels(16)) {
    LOG_ERR("PDR", "setGreyLevels(16) FAILED!");
  }

  LOG_INF("PDR", "Setting quality...");
  _painter.setQuality(EPD_Painter::Quality::QUALITY_HIGH);

  auto& cfg = EPD_Painter_Access::getMutableConfig(_painter);
  cfg.g16_pass_us_normal = 15000; // Fast pass for normal turns
  cfg.g16_pass_us_high   = 30000; // Thorough pass for ghost clearing

  LOG_INF("PDR", "Allocating planes...");
  size_t planeBytes = static_cast<size_t>(_wb) * _h;
  if (!_lsb) _lsb = (uint8_t*)heap_caps_malloc(planeBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!_msb) _msb = (uint8_t*)heap_caps_malloc(planeBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!_lsb || !_msb) {
    LOG_ERR("PDR", "FAILED to allocate grayscale planes in SPIRAM!");
    if (!_lsb) _lsb = (uint8_t*)malloc(planeBytes);
    if (!_msb) _msb = (uint8_t*)malloc(planeBytes);
  }
  if (_lsb) memset(_lsb, 0x00, planeBytes);
  if (_msb) memset(_msb, 0x00, planeBytes);

  if (!_painterFb) {
     const size_t fbSize = static_cast<size_t>(_w) * _h;
     _painterFb = (uint8_t*)heap_caps_aligned_alloc(16, fbSize, MALLOC_CAP_SPIRAM);
     if (_painterFb) {
       memset(_painterFb, 0x00, fbSize); // Start White (0)
       LOG_INF("PDR", "Allocated %u bytes for painter FB in SPIRAM", fbSize);
     } else {
       LOG_ERR("PDR", "FAILED to allocate %u bytes for painter FB in SPIRAM! Falling back to DRAM...", fbSize);
       _painterFb = (uint8_t*)heap_caps_aligned_alloc(16, fbSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
       if (_painterFb) {
         memset(_painterFb, 0x00, fbSize);
         LOG_INF("PDR", "Allocated %u bytes for painter FB in DRAM", fbSize);
       } else {
         LOG_ERR("PDR", "TOTAL OOM: Could not allocate painter FB!");
       }
     }
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

  uint32_t levelCounts[16] = {0};
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

        // CrossPoint 4-level mapping:
        // Value 0 (Black): b=0, m=0, l=0
        // Value 1 (Dark Gray): b=0, m=1, l=1
        // Value 2 (Light Gray): b=0, m=1, l=0
        // Value 3 (White): b=1, m=0, l=0

        // Smooth priority: Use grayscale planes if they have data, ignore BW dither.
        bool l_inked = (l & mask);
        bool m_inked = (m & mask);
        bool b_inked = !(b & mask);

        if (b_inked) {
          if (m_inked && l_inked) {
            level = 14; // Value 1: Dark Gray
          } else if (m_inked) {
            level = 11; // Value 2: Light Gray
          } else {
            level = 15; // Value 0: Black
          }
        } else {
          level = 0; // Value 3: White
        }

        drow[bx * 8 + bit] = level;
        levelCounts[level & 0xF]++;
      }
    }
    if (y % 50 == 0) yield();
  }
  LOG_INF("PDR", "syncToPainter: Levels [0]=%u [11]=%u [14]=%u [15]=%u",
          (unsigned)levelCounts[0], (unsigned)levelCounts[11], (unsigned)levelCounts[14], (unsigned)levelCounts[15]);
}

void PainterDriver::syncToPainter8Bit(const uint8_t* grayBuf) {
  if (!_painterFb || !grayBuf) return;

  uint32_t levelCounts[16] = {0};
  for (uint32_t y = 0; y < _h; y++) {
    const uint8_t* srcRow = grayBuf + (static_cast<size_t>(y) * _w);
    uint8_t* dstRow = _painterFb + (static_cast<size_t>(y) * _w);

    for (uint32_t x = 0; x < _w; x++) {
      int16_t targetLum = srcRow[x];

      // H716_LEVEL_LUM is sorted 255 (level 0) down to 0 (level 15).
      uint8_t level = 0;
      if (targetLum >= 250) {
        level = 0;
      } else if (targetLum <= 5) {
        level = 15;
      } else {
        // Direct linear mapping to 16 levels to avoid metallic grain
        // targetLum 255 -> 0, targetLum 0 -> 15
        level = 15 - (targetLum * 15 / 255);
      }

      dstRow[x] = level;
      levelCounts[level & 0xF]++;
    }
    if (y % 50 == 0) yield();
  }
  LOG_INF("PDR", "syncToPainter8Bit: [0]=%u [15]=%u (Linear 16-level)",
          (unsigned)levelCounts[0], (unsigned)levelCounts[15]);
}

void PainterDriver::display(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, RefreshMode mode, bool turnOff) {
  syncToPainter(fb, nullptr, nullptr);

  const bool forceClear = (mode == RefreshMode::Full || _ghostClearCounter >= GHOST_CLEAR_INTERVAL);

  if (forceClear) {
    LOG_INF("PDR", "Periodic ghost clear triggered (BW)");
    _painter.setQuality(EPD_Painter::Quality::QUALITY_HIGH);
    _painter.clear();
    _ghostClearCounter = 0;
  } else {
    _painter.setQuality(EPD_Painter::Quality::QUALITY_NORMAL);
    if (mode != RefreshMode::Fast) _ghostClearCounter++;
  }

  _painter.paint(_painterFb);
  if (turnOff) _painter.shutdown();
}

void PainterDriver::copyGrayscaleLsb(EpdBus& bus, const uint8_t* lsb) {
  if (lsb && _lsb) memcpy(_lsb, lsb, static_cast<size_t>(_wb) * _h);
}

void PainterDriver::copyGrayscaleMsb(EpdBus& bus, const uint8_t* msb) {
  if (msb && _msb) memcpy(_msb, msb, static_cast<size_t>(_wb) * _h);
}

void PainterDriver::writeGrayscalePlaneStrip(EpdBus& bus, GrayPlane plane, const uint8_t* rows, uint16_t yStart,
                                             uint16_t numRows) {
  uint8_t* dst = (plane == GrayPlane::Lsb) ? _lsb : _msb;
  if (!dst || !rows) return;
  const size_t offset = static_cast<size_t>(yStart) * _wb;
  const size_t bytes = static_cast<size_t>(numRows) * _wb;
  memcpy(dst + offset, rows, bytes);
}

void PainterDriver::displayGray(EpdBus& bus, const uint8_t* fb, bool turnOff, const unsigned char* lut, bool factoryMode) {
  LOG_INF("PDR", "displayGray called: fb=%p lsb=%p msb=%p (counter=%d)", fb, _lsb, _msb, _ghostClearCounter);
  syncToPainter(fb, _lsb, _msb);

  const bool forceClear = (factoryMode && _ghostClearCounter >= GHOST_CLEAR_INTERVAL); // Only clear if requested AND interval reached

  if (forceClear) {
    LOG_INF("PDR", "Periodic ghost clear triggered (Gray)");
    _painter.setQuality(EPD_Painter::Quality::QUALITY_HIGH);
    _painter.clear();
    _ghostClearCounter = 0;
  } else {
    _painter.setQuality(EPD_Painter::Quality::QUALITY_NORMAL);
    _ghostClearCounter++;
  }

  _painter.paint(_painterFb);
  if (turnOff) _painter.shutdown();
}

void PainterDriver::displayGray8Bit(EpdBus& bus, const uint8_t* grayBuf, RefreshMode mode, bool turnOff) {
  syncToPainter8Bit(grayBuf);
  if (mode == RefreshMode::Full) {
    _painter.clear();
    yield();
  }
  _painter.paint(_painterFb);
  if (turnOff) _painter.shutdown();
}

void PainterDriver::cleanupGrayscaleBuffers(EpdBus& bus, const uint8_t* bw) {
  const size_t planeBytes = static_cast<size_t>(_wb) * _h;
  if (_lsb) memset(_lsb, 0x00, planeBytes);
  if (_msb) memset(_msb, 0x00, planeBytes);
}

PanelDriver& painterDriver() {
  static PainterDriver instance;
  return instance;
}

} // namespace freeink
