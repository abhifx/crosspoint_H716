#include "BoardT5H716.h"

#include <InputManager.h>
#include <Logging.h>
#include <SPI.h>
#include <Wire.h>
#include <cassert>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace BoardT5H716 {
namespace {
SemaphoreHandle_t i2cMutex = nullptr;

SemaphoreHandle_t ensureI2CMutex() {
  if (i2cMutex == nullptr) {
    i2cMutex = xSemaphoreCreateRecursiveMutex();
    assert(i2cMutex != nullptr && "Failed to create I2C mutex");
  }
  return i2cMutex;
}

static uint8_t current_sr_state = 0;
static bool chargerPresent = false;

uint8_t inputButtonHook() {
  if (readButton()) {
    LOG_DBG("HW", "Button GPIO 21 pressed");
    return static_cast<uint8_t>(1U << InputManager::BTN_DOWN);
  }
  return 0;
}
}  // namespace

ScopedI2CLock::ScopedI2CLock() {
  xSemaphoreTakeRecursive(ensureI2CMutex(), portMAX_DELAY);
  locked_ = true;
}

ScopedI2CLock::~ScopedI2CLock() {
  if (locked_) {
    xSemaphoreGiveRecursive(ensureI2CMutex());
    locked_ = false;
  }
}

void pushShiftRegister(uint8_t data) {
  current_sr_state = data;
  pinMode(H716_SR_DATA, OUTPUT);
  pinMode(H716_SR_CLK, OUTPUT);
  pinMode(H716_SR_STR, OUTPUT);

  digitalWrite(H716_SR_STR, LOW);
  shiftOut(H716_SR_DATA, H716_SR_CLK, MSBFIRST, data);
  digitalWrite(H716_SR_STR, HIGH);
  delayMicroseconds(10);
  digitalWrite(H716_SR_STR, LOW);
}

void setSrBit(uint8_t bit, bool high) {
  if (high) current_sr_state |= bit;
  else current_sr_state &= ~bit;
  pushShiftRegister(current_sr_state);
}

void beginI2C() {
  ensureI2CMutex();
  Wire.begin(18, 17);
  Wire.setClock(400000);
  Wire.setTimeOut(50);

  // Probe for BQ25896 charger (0x6B) on main bus
  Wire.beginTransmission(0x6B);
  chargerPresent = (Wire.endTransmission() == 0);
  if (chargerPresent) {
    LOG_INF("HW", "BQ25896 charger detected at 0x6B");
  } else {
    LOG_INF("HW", "BQ25896 charger not found at 0x6B");
  }
}

void prepareSdBus() {
  pinMode(T5H716_SD_CS, OUTPUT);
  digitalWrite(T5H716_SD_CS, HIGH);
  SPI.begin(T5H716_SPI_SCLK, T5H716_SPI_MISO, T5H716_SPI_MOSI, T5H716_SD_CS);
}

void begin() {
  LOG_INF("HW", "LilyGo T5-H716 Standard Boot: Initializing...");

  // 1. Initialize Shift Register for Power
  pushShiftRegister(H716_SR_BASE);
  delay(300);

  // 2. Start main I2C for RTC/Touch
  beginI2C();

  pinMode(T5H716_BOOT_BTN, INPUT_PULLUP);
  prepareSdBus();

  // H716 Standard button on GPIO 21
  pinMode(21, INPUT_PULLUP);

  InputManager::setButtonHook(inputButtonHook);
}

void deinitForSleep() {
  // Disable display power rails and output enable via shift register
  pushShiftRegister(current_sr_state & ~(SR_BIT_POS_PWR | SR_BIT_NEG_PWR | SR_BIT_OE));

  pinMode(T5H716_SD_CS, INPUT);
}

bool readButton() {
  // H716 Standard button on GPIO 21
  return digitalRead(21) == LOW;
}

bool isUsbConnected() {
  // Use GPIO 43 for H716 Standard USB detection instead of I2C to avoid bus noise/errors
  if (BoardConfig::ACTIVE.usbDetect >= 0) {
    return digitalRead(BoardConfig::ACTIVE.usbDetect) == HIGH;
  }
  return false;
}

}  // namespace BoardT5H716
