#include "time_utils.h"
#include <WiFi.h>
#include <time.h>

// NTP syncing and RTC mirroring.
extern bool timeValid;
extern struct tm timeinfo;
extern bool rtcAvailable;
extern RTC_DS3231 rtc;

// CHANGE HERE: NTP resync interval (ms).
static const unsigned long NTP_SYNC_INTERVAL = 3600000; // 1h
static unsigned long lastNTPSyncTime = 0;

// Get current time, preferring RTC, falling back to System/NTP
DateTime getCurrentDateTime() {
  if (rtcAvailable) {
    return rtc.now();
  } else if (getLocalTime(&timeinfo)) {
    return DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
  return DateTime(2000, 1, 1, 0, 0, 0);
}

// Initial time sync sequence at boot
void syncTimeAtBoot() {
  // CHANGE HERE: timezone string + NTP servers.
  configTzTime("IST-2IDT,M3.4.4/26,M10.5.0",
               "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for time sync...");

  int retry = 0;
  const int retryCount = 20;
  while (!getLocalTime(&timeinfo) && retry++ < retryCount) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (retry < retryCount) {
    timeValid = true;
    Serial.printf("Time synced: %02d:%02d:%02d\n",
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    if (rtcAvailable) {
      rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    }
  } else {
    timeValid = rtcAvailable;
    Serial.println("Failed to get time from NTP");
  }
}

// Periodic check to keep RTC in sync with NTP
void tickTimeSync() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastNTPSyncTime <= NTP_SYNC_INTERVAL) return;

  if (getLocalTime(&timeinfo)) {
    timeValid = true;
    if (rtcAvailable) {
      rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    }
    Serial.println("NTP time resynced to RTC");
  }
  lastNTPSyncTime = millis();
}
