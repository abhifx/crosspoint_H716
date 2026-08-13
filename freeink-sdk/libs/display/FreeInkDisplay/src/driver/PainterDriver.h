#pragma once

#include "PanelDriver.h"
#include <EPD_Painter.h>

namespace freeink {

class PainterDriver final : public PanelDriver {
 public:
  PainterDriver();
  ~PainterDriver() override;

  uint32_t spiHz() const override { return 16000000; }
  BusyPolarity busyPolarity() const override { return BusyPolarity::ActiveHigh; }
  PanelGeometry geometry() const override;

  bool usesExternalBus() const override { return true; }

  void begin(EpdBus& bus) override;
  void deepSleep(EpdBus& bus) override;

  void display(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, RefreshMode mode, bool turnOff) override;

  // Grayscale support
  void copyGrayscaleLsb(EpdBus& bus, const uint8_t* lsb) override;
  void copyGrayscaleMsb(EpdBus& bus, const uint8_t* msb) override;
  void writeGrayscalePlaneStrip(EpdBus& bus, GrayPlane plane, const uint8_t* rows, uint16_t yStart,
                                uint16_t numRows) override;
  void displayGray(EpdBus& bus, const uint8_t* fb, bool turnOff, const unsigned char* lut, bool factoryMode) override;
  void displayGray8Bit(EpdBus& bus, const uint8_t* grayBuf, RefreshMode mode, bool turnOff) override;
  uint8_t* getInternalGrayBuffer() override { return _painterFb; }
  void cleanupGrayscaleBuffers(EpdBus& bus, const uint8_t* bw) override;
  bool supportsStripGrayscale() const override { return true; }

 private:
  EPD_Painter _painter;
  uint8_t* _painterFb = nullptr;
  uint8_t* _lsb = nullptr;
  uint8_t* _msb = nullptr;
  uint16_t _w, _h;
  uint32_t _wb;
  uint16_t _ghostClearCounter = 0;
  static constexpr uint16_t GHOST_CLEAR_INTERVAL = 8;

  uint8_t _lsbLUT[256];
  uint8_t _msbLUT[256];
  uint8_t _grayLUT[256];

  void syncToPainter(const uint8_t* bw, const uint8_t* lsb, const uint8_t* msb);
  void syncToPainter8Bit(const uint8_t* grayBuf);
};

PanelDriver& painterDriver();

} // namespace freeink
