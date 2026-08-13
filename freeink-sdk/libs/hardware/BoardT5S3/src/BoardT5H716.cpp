#include "BoardT5H716.h"

#include <InputManager.h>
#include <SPI.h>
#include <Wire.h>
#include <cassert>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/gpio.h>
#include <soc/gpio_struct.h>
#include <Logging.h>
#include <BoardConfig.h>

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

  // Keep STR LOW during shifting to avoid glitching storage register outputs
  digitalWrite(H716_SR_STR, LOW);
  shiftOut(H716_SR_DATA, H716_SR_CLK, MSBFIRST, data);
  // Pulse STR HIGH to transfer data from shift register to storage register
  digitalWrite(H716_SR_STR, HIGH);
  delayMicroseconds(10);
  digitalWrite(H716_SR_STR, LOW);
}

void pushShiftRegisterFast(uint8_t data) {
  current_sr_state = data;
  // Fast GPIO access for ESP32-S3
  // STR=0, CLK=12, DATA=13
  const uint32_t str = (1U << 0);
  const uint32_t clk = (1U << 12);
  const uint32_t dat = (1U << 13);

  // Pull STR LOW during shift
  GPIO.out_w1tc = str;
  ets_delay_us(1);
  for (int i = 0; i < 8; i++) {
    if (data & 0x80) GPIO.out_w1ts = dat;
    else GPIO.out_w1tc = dat;
    ets_delay_us(1);
    GPIO.out_w1ts = clk;
    ets_delay_us(1);
    GPIO.out_w1tc = clk;
    data <<= 1;
  }
  ets_delay_us(1);
  // Pulse STR HIGH
  GPIO.out_w1ts = str;
  ets_delay_us(1);
  GPIO.out_w1tc = str;
}

void setSrBit(uint8_t bit, bool high) {
  if (high) current_sr_state |= bit;
  else current_sr_state &= ~bit;
  pushShiftRegister(current_sr_state);
}

void beginI2C() {
  ensureI2CMutex();
  LOG_INF("H716", "Initializing shared I2C bus (SDA=%d SCL=%d)...", T5H716_SDA, T5H716_SCL);
  Wire.begin(T5H716_SDA, T5H716_SCL);
  Wire.setClock(T5H716_I2C_FREQ);
  Wire.setTimeOut(100); // Higher timeout for charger/touch sharing

  BoardConfig::i2cBegun = true;
}

void prepareSdBus() {
  pinMode(T5H716_SD_CS, OUTPUT);
  digitalWrite(T5H716_SD_CS, HIGH);
  SPI.begin(T5H716_SPI_SCLK, T5H716_SPI_MISO, T5H716_SPI_MOSI, T5H716_SD_CS);
}

void begin() {
  // Reset any previous sleep hold on the Shift Register strobe (GPIO 0 / BOOT)
  // to allow it to be driven by pushShiftRegister.
  gpio_hold_dis((gpio_num_t)H716_SR_STR);

  // Initialize Shift Register for Power
  // This enables OE, MODE, SCAN, STV, NEG_PWR, POS_PWR
  pushShiftRegister(H716_SR_BASE);

  // CRITICAL: Give the power rails and I2C chips time to fully stabilize.
  // The ED047TC1 rails draw a massive inrush current.
  delay(800);

  beginI2C();
  pinMode(T5H716_BOOT_BTN, INPUT_PULLUP);

  // Hardware USB detect pin
  pinMode(43, INPUT);

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
  // Check BQ25896 charger status (VBUS_STAT in REG0B) over I2C
  if (!BoardConfig::i2cBegun) return false;

  ScopedI2CLock lock;
  Wire.beginTransmission(0x6B); // BQ25896
  Wire.write(0x0B);             // REG0B
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(0x6B, 1) < 1) return false;
  uint8_t status = Wire.read();
  // VBUS_STAT is in bits [7:5]. If not 000, VBUS is present.
  return (status >> 5) != 0;
}

}  // namespace BoardT5H716
