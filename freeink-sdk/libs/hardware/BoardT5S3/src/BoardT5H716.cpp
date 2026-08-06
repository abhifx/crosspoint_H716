#include "BoardT5H716.h"

#include <InputManager.h>
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

uint8_t inputButtonHook() { return readButton() ? static_cast<uint8_t>(1U << InputManager::BTN_DOWN) : 0; }
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
  Wire.begin(T5H716_SDA, T5H716_SCL);
  Wire.setClock(T5H716_I2C_FREQ);
  Wire.setTimeOut(50);
}

void prepareSdBus() {
  pinMode(T5H716_SD_CS, OUTPUT);
  digitalWrite(T5H716_SD_CS, HIGH);
  SPI.begin(T5H716_SPI_SCLK, T5H716_SPI_MISO, T5H716_SPI_MOSI, T5H716_SD_CS);
}

void begin() {
  // Initialize Shift Register for Power
  pushShiftRegister(H716_SR_BASE);
  delay(300);

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

  // Latch the shift register control pins to maintain the state through sleep
  const gpio_num_t srPins[] = {
      static_cast<gpio_num_t>(H716_SR_DATA),
      static_cast<gpio_num_t>(H716_SR_CLK),
      static_cast<gpio_num_t>(H716_SR_STR)
  };
  for (const auto pin : srPins) {
    gpio_hold_en(pin);
  }

  pinMode(T5H716_SD_CS, INPUT);
}

bool readButton() {
  // H716 Standard button on GPIO 21
  return digitalRead(21) == LOW;
}

}  // namespace BoardT5H716
