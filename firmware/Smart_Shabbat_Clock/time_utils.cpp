#include "time_utils.h"
#include <WiFi.h>
#include <time.h>

// NTP syncing and RTC mirroring.
extern bool timeValid;
extern uint8_t relayMode;
void setRelayToLastEvent();
extern struct tm timeinfo;
extern bool rtcAvailable;
extern RTC_DS3231 rtc;

// CHANGE HERE: NTP resync intervals (ms).
static const unsigned long NTP_SYNC_INTERVAL = 3600000; // 1h when time is valid
static const unsigned long NTP_RETRY_INTERVAL = 15000;  // 15s while time is invalid
static unsigned long lastNTPSyncTime = 0;
static bool ntpConfigured = false;

static void configureNtpIfNeeded() {
  if (ntpConfigured) return;
  configTzTime("IST-2IDT,M3.4.4/26,M10.5.0",
               "pool.ntp.org", "time.nist.gov");
  ntpConfigured = true;
}

// Get current time, preferring RTC, falling back to System/NTP
DateTime getCurrentDateTime() {
  if (rtcAvailable) {
    return rtc.now();
  } else if (WiFi.status() == WL_CONNECTED && getLocalTime(&timeinfo, 200)) {
    return DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
  return DateTime(2000, 1, 1, 0, 0, 0);
}

// Initial time sync sequence at boot
void syncTimeAtBoot() {
  // If there is no WiFi connection, don't block on NTP at boot.
  if (WiFi.status() != WL_CONNECTED) {
    // RTC can still provide valid time even without WiFi (if it didn't lose power).
    timeValid = rtcAvailable && !rtc.lostPower();
    Serial.println("Skip NTP at boot: no WiFi. Using RTC if reliable.");
    return;
  }

  configureNtpIfNeeded();
  Serial.println("Waiting for time sync...");

  int retry = 0;
  const int retryCount = 20;
  while (!getLocalTime(&timeinfo) && retry++ < retryCount) {
    Serial.print(".");
    delay(200);
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
    timeValid = rtcAvailable && !rtc.lostPower();
    Serial.println("Failed to get time from NTP");
  }
}

// Periodic check to keep RTC in sync with NTP
void tickTimeSync() {
  if (WiFi.status() != WL_CONNECTED) return;
  configureNtpIfNeeded();
  const unsigned long interval = timeValid ? NTP_SYNC_INTERVAL : NTP_RETRY_INTERVAL;
  if (lastNTPSyncTime != 0 && (millis() - lastNTPSyncTime <= interval)) return;

  bool wasValid = timeValid;
  if (getLocalTime(&timeinfo)) {
    timeValid = true;
    if (rtcAvailable) {
      rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    }
    if (!wasValid && relayMode == 2) {
      setRelayToLastEvent();
    }
    Serial.println("NTP time resynced to RTC");
  }
  lastNTPSyncTime = millis();
}
