#include <BoardT5H716.h>
#include <LgfxEpdConfig.h>
#include <Wire.h>

namespace {

#define LUT_MAKE(d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, da, db, dc, dd, de, df) \
  (uint32_t)((d0 << 0) | (d1 << 2) | (d2 << 4) | (d3 << 6) | (d4 << 8) | (d5 << 10) | (d6 << 12) | \
             (d7 << 14) | (d8 << 16) | (d9 << 18) | (da << 20) | (db << 22) | (dc << 24) | \
             (dd << 26) | (de << 28) | (df << 30))

constexpr uint32_t kFastLut[] = {
    LUT_MAKE(2, 1, 1, 1, 1, 1, 1, 3, 3, 2, 2, 2, 2, 2, 2, 1),
    LUT_MAKE(2, 3, 1, 1, 1, 1, 3, 3, 3, 3, 2, 2, 2, 2, 3, 1),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    ~0u,
    0u,
};

#undef LUT_MAKE

bool prepareEpdPower() {
  // EP_CKV (GPIO 38) is the vertical clock used in power sequence
  pinMode(38, OUTPUT);
  digitalWrite(38, HIGH);
  return true;
}

void epdPowerOff() {
  // Disable display power rails and output enable via shift register
  BoardT5H716::pushShiftRegister(BoardT5H716::H716_SR_BASE & ~BoardT5H716::SR_BIT_OE);
}

bool epdPowerOn() {
  // Ensure STV is inactive (HIGH in SR)
  BoardT5H716::pushShiftRegister(BoardT5H716::H716_SR_BASE);
  return true;
}

}  // namespace

namespace freeink {

const LgfxEpdConfig& lilygoT5H716LgfxConfig() {
  static const LgfxEpdConfig cfg = {
      {EP_D0, EP_D1, EP_D2, EP_D3, EP_D4, EP_D5, EP_D6, EP_D7},
      EP_STH,
      -1, // pinSpv handled by SR
      -1, // pinOe handled by SR
      -1, // pinLe handled by SR
      EP_CKH,
      EP_CKV,
      T5H716_EPD_DUMMY_DC, // pinPwr -> dummy DC (GPIO 14)
      16000000,
      8,
      0,
      {&prepareEpdPower, &epdPowerOn, &epdPowerOff},
      nullptr,
      0,
      nullptr,
      0,
      kFastLut,
      sizeof(kFastLut) / sizeof(kFastLut[0]),
      kFastLut,
      sizeof(kFastLut) / sizeof(kFastLut[0]),
  };
  return cfg;
}

}  // namespace freeink
