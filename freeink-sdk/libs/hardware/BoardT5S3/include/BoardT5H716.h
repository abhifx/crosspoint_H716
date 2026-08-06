#pragma once

#include <Arduino.h>
#include "BoardT5H716Pins.h"

namespace BoardT5H716 {

class ScopedI2CLock {
 public:
  ScopedI2CLock();
  ~ScopedI2CLock();

  ScopedI2CLock(const ScopedI2CLock&) = delete;
  ScopedI2CLock& operator=(const ScopedI2CLock&) = delete;

 private:
  bool locked_ = false;
};

void begin();
void beginI2C();
void pushShiftRegister(uint8_t data);
void setSrBit(uint8_t bit, bool high);
void prepareSdBus();

// Shift Register Bits (Official V2.3/V2.4 Wiring)
constexpr uint8_t SR_BIT_LE      = (1 << 0);
constexpr uint8_t SR_BIT_PWR_DIS = (1 << 1);
constexpr uint8_t SR_BIT_POS_PWR = (1 << 2);
constexpr uint8_t SR_BIT_NEG_PWR = (1 << 3);
constexpr uint8_t SR_BIT_STV     = (1 << 4);
constexpr uint8_t SR_BIT_SCAN    = (1 << 5);
constexpr uint8_t SR_BIT_MODE    = (1 << 6);
constexpr uint8_t SR_BIT_OE      = (1 << 7);

// Base state for H716: OE, MODE, SCAN, STV, NEG_PWR, POS_PWR enabled
constexpr uint8_t H716_SR_BASE = (SR_BIT_OE | SR_BIT_MODE | SR_BIT_SCAN | SR_BIT_STV | SR_BIT_NEG_PWR | SR_BIT_POS_PWR);

void deinitForSleep();
bool readButton();

}  // namespace BoardT5H716
