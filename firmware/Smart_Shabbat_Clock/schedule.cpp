#include "schedule.h"
#include "time_utils.h"
#include <RTClib.h>
#include <Preferences.h>
#include <algorithm>

extern Preferences prefs;
extern bool timeValid;
extern uint8_t relayMode;
void setLocalRelayState(bool);

ScheduleEntry schedule[MAX_EVENTS];
uint8_t scheduleCount = 0;
uint32_t scheduleRevision = 0;

static const uint16_t MINUTES_PER_DAY = 24 * 60;

// ---------------------- Storage ----------------------

void saveSchedule() {
  Serial.println("Saving schedule to Preferences...");
  prefs.begin("sched", false);
  scheduleRevision++;
  prefs.putUChar("count", scheduleCount);
  prefs.putUInt("revision", scheduleRevision);
  if (scheduleCount > 0) {
    prefs.putBytes("table", schedule, sizeof(ScheduleEntry) * scheduleCount);
  }
  prefs.end();
  Serial.println("Schedule saved.");
}

void loadSchedule() {
  Serial.println("Loading schedule from Preferences...");
  prefs.begin("sched", true);
  scheduleRevision = prefs.getUInt("revision", 0);
  uint8_t cnt = prefs.getUChar("count", 0);
  if (cnt > MAX_EVENTS) cnt = MAX_EVENTS;
  if (cnt > 0 && prefs.getBytes("table", schedule, sizeof(ScheduleEntry) * cnt) == sizeof(ScheduleEntry) * cnt) {
    scheduleCount = cnt;
    normalizeSchedule();
    Serial.printf("Loaded %d schedule entries.\n", scheduleCount);
  } else {
    scheduleCount = 0;
    Serial.println("No saved schedule found.");
  }
  prefs.end();
}

void clearScheduleStorage() {
  Serial.println("Clearing schedule...");
  prefs.begin("sched", false);
  prefs.clear();
  prefs.putUInt("revision", 0);
  prefs.end();
  scheduleCount = 0;
  scheduleRevision = 0;
  Serial.println("Schedule cleared.");
}

// ---------------------- Schedule Logic ----------------------

uint16_t minuteOfWeek(const ScheduleEntry &entry) {
  return minuteOfWeek(entry.day, entry.hour, entry.minute);
}

uint16_t minuteOfWeek(uint8_t day, uint8_t hour, uint8_t minute) {
  return (uint16_t)day * MINUTES_PER_DAY + (uint16_t)hour * 60 + minute;
}

int compareScheduleEntries(const ScheduleEntry &a, const ScheduleEntry &b) {
  uint16_t aMinute = minuteOfWeek(a);
  uint16_t bMinute = minuteOfWeek(b);
  if (aMinute < bMinute) return -1;
  if (aMinute > bMinute) return 1;
  return 0;
}

static int findEventAtOrBeforeMinute(uint16_t targetMinute, bool includeEqual) {
  if (scheduleCount == 0) return -1;

  int bestIdx = -1;
  for (uint8_t i = 0; i < scheduleCount; i++) {
    uint16_t eventMinute = minuteOfWeek(schedule[i]);
    bool beforeTarget = includeEqual ? eventMinute <= targetMinute : eventMinute < targetMinute;
    if (!beforeTarget) continue;
    if (bestIdx < 0 || eventMinute > minuteOfWeek(schedule[bestIdx])) {
      bestIdx = i;
    }
  }

  if (bestIdx >= 0) return bestIdx;

  bestIdx = 0;
  for (uint8_t i = 1; i < scheduleCount; i++) {
    if (minuteOfWeek(schedule[i]) > minuteOfWeek(schedule[bestIdx])) {
      bestIdx = i;
    }
  }
  return bestIdx;
}

int findPreviousEffectiveEventIndex(uint8_t day, uint8_t hour, uint8_t minute) {
  return findEventAtOrBeforeMinute(minuteOfWeek(day, hour, minute), false);
}

int findLastEventBeforeNow() {
  if (!timeValid || scheduleCount == 0) return -1;

  DateTime now = getCurrentDateTime();
  return findEventAtOrBeforeMinute(minuteOfWeek(now.dayOfTheWeek(), now.hour(), now.minute()), true);
}

static int findNextEventAfterNow() {
  if (!timeValid || scheduleCount == 0) return -1;

  DateTime now = getCurrentDateTime();
  uint16_t currentMinute = minuteOfWeek(now.dayOfTheWeek(), now.hour(), now.minute());
  int bestIdx = -1;

  for (uint8_t i = 0; i < scheduleCount; i++) {
    uint16_t eventMinute = minuteOfWeek(schedule[i]);
    if (eventMinute <= currentMinute) continue;
    if (bestIdx < 0 || eventMinute < minuteOfWeek(schedule[bestIdx])) {
      bestIdx = i;
    }
  }

  if (bestIdx >= 0) return bestIdx;

  bestIdx = 0;
  for (uint8_t i = 1; i < scheduleCount; i++) {
    if (minuteOfWeek(schedule[i]) < minuteOfWeek(schedule[bestIdx])) {
      bestIdx = i;
    }
  }
  return bestIdx;
}

void sortSchedule() {
  Serial.println("Sorting schedule entries...");
  std::stable_sort(schedule, schedule + scheduleCount, [](const ScheduleEntry &a, const ScheduleEntry &b) {
    return compareScheduleEntries(a, b) < 0;
  });
  Serial.println("Schedule sorted.");
}

void normalizeSchedule() {
  if (scheduleCount <= 1) {
    sortSchedule();
    return;
  }

  sortSchedule();

  ScheduleEntry uniqueEvents[MAX_EVENTS];
  uint8_t uniqueCount = 0;

  for (uint8_t i = 0; i < scheduleCount; i++) {
    if (uniqueCount > 0 && minuteOfWeek(schedule[i]) == minuteOfWeek(uniqueEvents[uniqueCount - 1])) {
      uniqueEvents[uniqueCount - 1] = schedule[i];
    } else {
      uniqueEvents[uniqueCount++] = schedule[i];
    }
  }

  if (uniqueCount <= 1) {
    if (uniqueCount == 1) schedule[0] = uniqueEvents[0];
    scheduleCount = uniqueCount;
    return;
  }

  ScheduleEntry normalized[MAX_EVENTS];
  uint8_t normalizedCount = 0;

  for (uint8_t i = 0; i < uniqueCount; i++) {
    uint8_t previousIndex = (i == 0) ? uniqueCount - 1 : i - 1;
    if (uniqueEvents[i].state != uniqueEvents[previousIndex].state) {
      normalized[normalizedCount++] = uniqueEvents[i];
    }
  }

  if (normalizedCount == 0) {
    normalized[normalizedCount++] = uniqueEvents[0];
  }

  for (uint8_t i = 0; i < normalizedCount; i++) schedule[i] = normalized[i];
  scheduleCount = normalizedCount;
}

void applyScheduleLogic() {
  if (!timeValid) return;
  if (relayMode != 2 || scheduleCount == 0) return;

  int lastIdx = findLastEventBeforeNow();
  if (lastIdx >= 0) {
    setLocalRelayState(schedule[lastIdx].state);
  }
}

// ---------------------- Relay Logic ----------------------

void setRelayOppositeToNextEvent() {
  if (!timeValid || scheduleCount == 0) return;
  Serial.println("Setting relay opposite to next scheduled event...");

  int nextIdx = findNextEventAfterNow();
  if (nextIdx >= 0) {
    setLocalRelayState(!schedule[nextIdx].state);
    Serial.printf("Relay set opposite to event at %02d:%02d on day %d\n",
                  schedule[nextIdx].hour, schedule[nextIdx].minute, schedule[nextIdx].day);
  }
}

void setRelayToLastEvent() {
  if (!timeValid || scheduleCount == 0) return;
  Serial.println("Setting relay to last event state...");

  int lastIdx = findLastEventBeforeNow();
  if (lastIdx >= 0) {
    setLocalRelayState(schedule[lastIdx].state);
    Serial.printf("Relay set to last event at %02d:%02d on day %d\n",
                  schedule[lastIdx].hour, schedule[lastIdx].minute, schedule[lastIdx].day);
  }
}
