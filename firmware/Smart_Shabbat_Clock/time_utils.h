#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <time.h>
#include <RTClib.h>

// Time utilities: current time selection, NTP sync, and RTC mirroring.
DateTime getCurrentDateTime();
void ensureLocalTimezoneConfigured();
void syncTimeAtBoot();
void tickTimeSync();
void tickRtcDstCorrection();
void updateRtcDstStateFromLocalTime(const struct tm& localTime);

extern bool timeValid;
extern struct tm timeinfo;
extern bool rtcAvailable;
extern RTC_DS3231 rtc;

#endif // TIME_UTILS_H
