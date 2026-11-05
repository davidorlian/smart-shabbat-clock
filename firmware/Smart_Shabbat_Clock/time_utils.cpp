#include "time_utils.h"
#include <Preferences.h>
#include <WiFi.h>
#include <stdlib.h>
#include <time.h>

// NTP syncing and RTC mirroring.
extern bool timeValid;
extern uint8_t relayMode;
void setRelayToLastEvent();
extern struct tm timeinfo;
extern bool rtcAvailable;
extern RTC_DS3231 rtc;
extern Preferences prefs;

// CHANGE HERE: NTP resync intervals (ms).
static const unsigned long NTP_SYNC_INTERVAL = 3600000; // 1h when time is valid
static const unsigned long NTP_RETRY_INTERVAL = 15000;  // 15s while time is invalid
static const unsigned long RTC_DST_CHECK_INTERVAL = 60000; // 1m
static const char* ISRAEL_TZ = "IST-2IDT,M3.4.4/26,M10.5.0";
static unsigned long lastNTPSyncTime = 0;
static unsigned long lastRtcDstCheck = 0;
static bool timezoneConfigured = false;
static bool ntpConfigured = false;

struct RtcDstState {
  bool known;
  bool active;
  uint16_t startYear;
  uint16_t endYear;
};

static bool isLeapYear(uint16_t year) {
  return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static uint8_t daysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t DAYS[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (month == 2 && isLeapYear(year)) return 29;
  return DAYS[month - 1];
}

static uint8_t nthWeekdayOfMonth(uint16_t year, uint8_t month, uint8_t weekday, uint8_t nth) {
  if (nth == 5) {
    uint8_t lastDay = daysInMonth(year, month);
    uint8_t lastDow = DateTime(year, month, lastDay, 0, 0, 0).dayOfTheWeek();
    return lastDay - ((lastDow + 7 - weekday) % 7);
  }

  uint8_t firstDow = DateTime(year, month, 1, 0, 0, 0).dayOfTheWeek();
  return 1 + ((weekday + 7 - firstDow) % 7) + (7 * (nth - 1));
}

static DateTime israelDstStart(uint16_t year) {
  // POSIX TZ M3.4.4/26: fourth Thursday in March at 26:00 = Friday 02:00.
  uint8_t fourthThursday = nthWeekdayOfMonth(year, 3, 4, 4);
  return DateTime(year, 3, fourthThursday + 1, 2, 0, 0);
}

static DateTime israelDstEnd(uint16_t year) {
  // POSIX TZ M10.5.0: last Sunday in October at 02:00.
  uint8_t lastSunday = nthWeekdayOfMonth(year, 10, 0, 5);
  return DateTime(year, 10, lastSunday, 2, 0, 0);
}

static bool isBefore(const DateTime& a, const DateTime& b) {
  return a.unixtime() < b.unixtime();
}

static bool isAtOrAfter(const DateTime& a, const DateTime& b) {
  return a.unixtime() >= b.unixtime();
}

static bool isIsraelDstActiveAtLocalTime(const DateTime& localTime) {
  uint16_t year = localTime.year();
  return isAtOrAfter(localTime, israelDstStart(year)) &&
         isBefore(localTime, israelDstEnd(year));
}

static RtcDstState loadRtcDstState() {
  RtcDstState state;
  prefs.begin("clock", true);
  state.known = prefs.getBool("dstKnown", false);
  state.active = prefs.getBool("dstActive", false);
  state.startYear = prefs.getUShort("dstStartY", 0);
  state.endYear = prefs.getUShort("dstEndY", 0);
  prefs.end();
  return state;
}

static void saveRtcDstState(const RtcDstState& state) {
  prefs.begin("clock", false);
  prefs.putBool("dstKnown", state.known);
  prefs.putBool("dstActive", state.active);
  prefs.putUShort("dstStartY", state.startYear);
  prefs.putUShort("dstEndY", state.endYear);
  prefs.end();
}

static void adjustRtcBySeconds(int32_t deltaSeconds) {
  DateTime now = rtc.now();
  int64_t adjustedUnix = (int64_t)now.unixtime() + deltaSeconds;
  rtc.adjust(DateTime((uint32_t)adjustedUnix));
}

void ensureLocalTimezoneConfigured() {
  if (timezoneConfigured) return;
  setenv("TZ", ISRAEL_TZ, 1);
  tzset();
  timezoneConfigured = true;
}

static void configureNtpIfNeeded() {
  if (ntpConfigured) return;
  ensureLocalTimezoneConfigured();
  configTzTime(ISRAEL_TZ, "pool.ntp.org", "time.nist.gov");
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

void updateRtcDstStateFromLocalTime(const struct tm& localTime) {
  uint16_t year = localTime.tm_year + 1900;
  if (year < 2020) return;

  DateTime localDateTime(year, localTime.tm_mon + 1, localTime.tm_mday,
                         localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
  bool active = localTime.tm_isdst > 0;
  if (localTime.tm_isdst < 0) {
    active = isIsraelDstActiveAtLocalTime(localDateTime);
  }

  RtcDstState state = loadRtcDstState();
  state.known = true;
  state.active = active;

  if (active) {
    state.startYear = year;
  } else if (isAtOrAfter(localDateTime, israelDstEnd(year))) {
    state.endYear = year;
  } else if (isBefore(localDateTime, israelDstStart(year)) && year > 2020) {
    state.endYear = year - 1;
  }

  saveRtcDstState(state);
}

void tickRtcDstCorrection() {
  if (!rtcAvailable || !timeValid) return;

  unsigned long nowMs = millis();
  if (lastRtcDstCheck != 0 && (nowMs - lastRtcDstCheck < RTC_DST_CHECK_INTERVAL)) return;
  lastRtcDstCheck = nowMs;

  DateTime now = rtc.now();
  uint16_t year = now.year();
  if (year < 2020) return;

  RtcDstState state = loadRtcDstState();
  if (!state.known) {
    state.known = true;
    state.active = isIsraelDstActiveAtLocalTime(now);
    if (state.active) {
      state.startYear = year;
    } else if (isAtOrAfter(now, israelDstEnd(year))) {
      state.endYear = year;
    } else if (isBefore(now, israelDstStart(year)) && year > 2020) {
      state.endYear = year - 1;
    }
    saveRtcDstState(state);
    Serial.printf("RTC DST state initialized: %s\n", state.active ? "IDT" : "IST");
    return;
  }

  DateTime start = israelDstStart(year);
  DateTime end = israelDstEnd(year);
  bool corrected = false;

  if (!state.active && state.startYear != year && isAtOrAfter(now, start) && isBefore(now, end)) {
    adjustRtcBySeconds(3600);
    state.active = true;
    state.startYear = year;
    corrected = true;
    Serial.println("RTC DST correction: +1 hour for Israel summer time");
  } else if (state.active && state.endYear != year && isAtOrAfter(now, end)) {
    adjustRtcBySeconds(-3600);
    state.active = false;
    state.endYear = year;
    corrected = true;
    Serial.println("RTC DST correction: -1 hour for Israel winter time");
  } else if (state.active && year > 2020 && state.endYear != (year - 1) && isBefore(now, start)) {
    adjustRtcBySeconds(-3600);
    state.active = false;
    state.endYear = year - 1;
    corrected = true;
    Serial.println("RTC DST correction: -1 hour for missed Israel winter transition");
  }

  if (corrected) {
    saveRtcDstState(state);
    if (relayMode == 2) {
      setRelayToLastEvent();
    }
  }
}

// Initial time sync sequence at boot
void syncTimeAtBoot() {
  ensureLocalTimezoneConfigured();

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
    updateRtcDstStateFromLocalTime(timeinfo);
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
    updateRtcDstStateFromLocalTime(timeinfo);
    if (!wasValid && relayMode == 2) {
      setRelayToLastEvent();
    }
    Serial.println("NTP time resynced to RTC");
  }
  lastNTPSyncTime = millis();
}
