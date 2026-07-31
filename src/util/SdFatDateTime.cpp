#include "TimeUtils.h"

#include "CrossPointState.h"

#include <Arduino.h>
// Include SdFat's date helpers without HalStorage.h, which aliases FsFile to HalFile
// and conflicts with SdFat's FsFile class in the same translation unit.
#include <common/FsDateTime.h>

#include <ctime>

namespace {

void sdFatDateTimeCallback(uint16_t* date, uint16_t* time, uint8_t* ms10) {
  // Keep this path cheap: do not touch the RTC I2C bus here. Boot/loop already
  // bridges RTC into the ESP system clock; Sync Day updates lastKnownValidTimestamp.
  uint32_t epoch = TimeUtils::getCurrentValidTimestamp();
  if (!TimeUtils::isClockValid(epoch)) {
    epoch = APP_STATE.lastKnownValidTimestamp;
  }

  if (!TimeUtils::isClockValid(epoch)) {
    // Match SdFat's placeholder when no wall clock is known yet.
    *date = FS_DATE(compileYear(), 1, 1);
    *time = FS_TIME(0, 0, 0);
    *ms10 = 0;
    return;
  }

  TimeUtils::configureTimezone();
  time_t currentTime = static_cast<time_t>(epoch);
  tm localTime = {};
  if (localtime_r(&currentTime, &localTime) == nullptr) {
    *date = FS_DATE(compileYear(), 1, 1);
    *time = FS_TIME(0, 0, 0);
    *ms10 = 0;
    return;
  }

  const int year = localTime.tm_year + 1900;
  if (year < 1980 || year > 2107) {
    *date = FS_DATE(compileYear(), 1, 1);
    *time = FS_TIME(0, 0, 0);
    *ms10 = 0;
    return;
  }

  *date = FS_DATE(static_cast<uint16_t>(year), static_cast<uint8_t>(localTime.tm_mon + 1),
                  static_cast<uint8_t>(localTime.tm_mday));
  *time = FS_TIME(static_cast<uint8_t>(localTime.tm_hour), static_cast<uint8_t>(localTime.tm_min),
                  static_cast<uint8_t>(localTime.tm_sec));
  *ms10 = (localTime.tm_sec & 1) ? 100 : 0;
}

}  // namespace

void TimeUtils::registerSdFatDateTimeCallback() { FsDateTime::setCallback(sdFatDateTimeCallback); }
