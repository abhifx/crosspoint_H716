#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>

#include <string>

/*
Define ENABLE_SERIAL_LOG to enable logging
Can be set in platformio.ini build_flags or as a compile definition

Define LOG_LEVEL to control log verbosity:
0 = ERR only
1 = ERR + INF
2 = ERR + INF + DBG
If not defined, defaults to 0
*/

#ifndef LOG_LEVEL
#define LOG_LEVEL 0
#endif

// Arduino Core 3.x (used in 6.13.0) has different Serial classes
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
#include <HWCDC.h>
static HWCDC& logSerial = Serial;
#define LOG_SERIAL_HAS_TX_TIMEOUT 1
#else
static HardwareSerial& logSerial = Serial;
#define LOG_SERIAL_HAS_TX_TIMEOUT 0
#endif

void logPrintf(const char* level, const char* origin, const char* format, ...);

#ifdef ENABLE_SERIAL_LOG
#if LOG_LEVEL >= 0
#define LOG_ERR(origin, format, ...) logPrintf("ERR", origin, format "\n", ##__VA_ARGS__)
#else
#define LOG_ERR(origin, format, ...)
#endif

#if LOG_LEVEL >= 1
#define LOG_INF(origin, format, ...) logPrintf("INF", origin, format "\n", ##__VA_ARGS__)
#else
#define LOG_INF(origin, format, ...)
#endif

#if LOG_LEVEL >= 2
#define LOG_DBG(origin, format, ...) logPrintf("DBG", origin, format "\n", ##__VA_ARGS__)
#else
#define LOG_DBG(origin, format, ...)
#endif
#else
#define LOG_DBG(origin, format, ...)
#define LOG_ERR(origin, format, ...)
#define LOG_INF(origin, format, ...)
#endif

std::string getLastLogs();
void clearLastLogs();
bool sanitizeLogHead();

class MySerialImpl : public Print {
 public:
  void begin(unsigned long baud) { logSerial.begin(baud); }
  operator bool() const { return logSerial; }
  size_t write(uint8_t b) override { return logSerial.write(b); }
  size_t write(const uint8_t* buffer, size_t size) override { return logSerial.write(buffer, size); }
  void flush() override { logSerial.flush(); }
  static MySerialImpl instance;
};

#ifdef Serial
#undef Serial
#endif
#define Serial MySerialImpl::instance
