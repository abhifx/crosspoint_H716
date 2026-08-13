#pragma once

#include <Arduino.h>

// LilyGO T5-4.7-S3 H716 (Standard / Touch)
// Fixed pins for H716 S3 based on official hardware specs

#define T5H716_WIDTH 960
#define T5H716_HEIGHT 540

// I2C Addresses
#define T5H716_GT911_ADDR 0x5D
#define T5H716_PCF85063_ADDR 0x51
#define T5H716_BQ25896_ADDR 0x6B
#define T5H716_BQ27220_ADDR 0x55
#define T5H716_TPS65185_ADDR 0x68 // Not present but defined for compatibility
#define T5H716_PCA9535_ADDR 0x20  // Not present but defined for compatibility

// I2C Pins (Touch GT911 & RTC PCF8563)
#define T5H716_SDA 18
#define T5H716_SCL 17
#define T5H716_I2C_FREQ 100000

// SD Card Pins (H716 S3)
#define T5H716_SD_CS 42
#define T5H716_SD_SCLK 11
#define T5H716_SD_MOSI 15
#define T5H716_SD_MISO 16

// SPI (Shared with SD)
#define T5H716_SPI_MISO (T5H716_SD_MISO)
#define T5H716_SPI_MOSI (T5H716_SD_MOSI)
#define T5H716_SPI_SCLK (T5H716_SD_SCLK)

// Touch Controller (GT911)
#define T5H716_TOUCH_SDA (T5H716_SDA)
#define T5H716_TOUCH_SCL (T5H716_SCL)
#define T5H716_TOUCH_INT 47
#define T5H716_TOUCH_RST -1

// RTC (PCF8563)
#define T5H716_RTC_SDA (T5H716_SDA)
#define T5H716_RTC_SCL (T5H716_SCL)

// H716 direct-GPIO panel bus (3 control lines only)
#define EP_CKH 41
#define EP_STH 40
#define EP_CKV 38

// Data Bus
#define EP_D0 8
#define EP_D1 1
#define EP_D2 2
#define EP_D3 3
#define EP_D4 4
#define EP_D5 5
#define EP_D6 6
#define EP_D7 7

// 74HCT4094D Shift Register (Official Wiring)
#define H716_SR_DATA 13
#define H716_SR_CLK  12
#define H716_SR_STR  0

// System Pins
#define T5H716_BOOT_BTN 0
#define T5H716_EPD_PWR_EN -1
#define T5H716_EPD_DUMMY_DC 14

namespace BoardT5H716Pins {
static constexpr uint16_t DisplayWidth = T5H716_WIDTH;
static constexpr uint16_t DisplayHeight = T5H716_HEIGHT;

static constexpr uint8_t I2cSda = T5H716_SDA;
static constexpr uint8_t I2cScl = T5H716_SCL;
static constexpr uint32_t I2cFreq = T5H716_I2C_FREQ;

static constexpr uint8_t SdCs = T5H716_SD_CS;
static constexpr uint8_t BootButton = T5H716_BOOT_BTN;
}  // namespace BoardT5H716Pins
