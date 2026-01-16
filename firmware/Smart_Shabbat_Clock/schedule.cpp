#include "schedule.h"
#include "time_utils.h"
#include <RTClib.h>
#include <Preferences.h>
#include <algorithm> // For std::sort

extern Preferences prefs;
extern bool timeValid;
extern uint8_t relayMode;
void setLocalRelayState(bool);

ScheduleEntry schedule[MAX_EVENTS];
uint8_t scheduleCount = 0;


// ---------------------- Storage ----------------------
// saveSchedule/loadSchedule/clearSchedule operate on NVS.
// After any modification we sort and (optionally) normalize.

void saveSchedule() {
  Serial.println("Saving schedule to Preferences...");
  prefs.begin("sched", false);
  prefs.putUChar("count", scheduleCount);
  if (scheduleCount > 0) {
    prefs.putBytes("table", schedule, sizeof(ScheduleEntry) * scheduleCount);
  }
  prefs.end();
  Serial.println("Schedule saved.");
}

void loadSchedule() {
  Serial.println("Loading schedule from Preferences...");
  prefs.begin("sched", true);
  uint8_t cnt = prefs.getUChar("count", 0);
  if (cnt > MAX_EVENTS) cnt = MAX_EVENTS;
  if (cnt > 0 && prefs.getBytes("table", schedule, sizeof(ScheduleEntry) * cnt) == sizeof(ScheduleEntry) * cnt) {
    scheduleCount = cnt;
    sortSchedule();
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
  prefs.clear(); // wipe namespace "sched"
  prefs.end();
  scheduleCount = 0;
  Serial.println("Schedule cleared.");
}

// ---------------------- Schedule Logic ----------------------
// sortSchedule orders by (day, hour, minute) ascending.
// normalizeSchedule compresses consecutive same-state events per day
// so no-ops are removed (keeps only state flips).
void sortSchedule() {
  // Sort events by Day -> Hour -> Minute
  Serial.println("Sorting schedule entries...");
  std::sort(schedule, schedule + scheduleCount, [](const ScheduleEntry &a, const ScheduleEntry &b) {
    if (a.day != b.day) return a.day < b.day;
    if (a.hour != b.hour) return a.hour < b.hour;
    return a.minute < b.minute;
  });
  Serial.println("Schedule sorted.");
}

// Remove redundant events (e.g. ON followed by ON) to save space
void normalizeSchedule() {
  if (scheduleCount <= 1) return;

  ScheduleEntry tmp[MAX_EVENTS];
  uint8_t out = 0;

  int lastDay   = -1;
  int lastState = -1;

  for (int i = 0; i < scheduleCount; i++) {
    const auto &e = schedule[i];

    if (e.day != lastDay) {
      // First event of a new day – always keep
      tmp[out++] = e;
      lastDay = e.day;
      lastState = e.state;
    } else {
      // Same day – keep only state flips to avoid redundant entries.
      if ((int)e.state != lastState) {
        tmp[out++] = e;
        lastState = e.state;
      }
    }
  }

  // Commit back
  for (int i = 0; i < out; i++) schedule[i] = tmp[i];
  scheduleCount = out;
}

// AUTO mode: find the latest event (<= now) for today and apply it.
// CHANGE HERE: adjust schedule behavior in AUTO mode.
// If nothing yet today, relay remains as-is; "setRelayToLastEvent()" at boot
// handles continuity across days.
void applyScheduleLogic() {
  if (!timeValid) return; 
  if (relayMode != 2 || scheduleCount == 0) return;

  DateTime now = getCurrentDateTime();
  uint8_t currentDay = now.dayOfTheWeek();
  uint8_t currentHour = now.hour();
  uint8_t currentMinute = now.minute();
  int lastIdx = -1;

  for (int i = 0; i < scheduleCount; i++) {
    const auto &e = schedule[i];
    if (e.day != currentDay) continue;
    if (e.hour < currentHour || (e.hour == currentHour && e.minute <= currentMinute)) {
      if (lastIdx < 0 ||
          e.hour > schedule[lastIdx].hour ||
          (e.hour == schedule[lastIdx].hour && e.minute > schedule[lastIdx].minute)) {
        lastIdx = i;
      }
    }
  }
  if (lastIdx >= 0) {
    setLocalRelayState(schedule[lastIdx].state);
  } 
}

// ---------------------- Relay Logic ----------------------
// Sets the relay opposite to the next scheduled event from "now" forward.
// Used when switching to AUTO mode to ensure correct initial state.
// Useful when switching to AUTO so that the next event will flip state.
void setRelayOppositeToNextEvent() {
  if (!timeValid || scheduleCount == 0) return;
  Serial.println("Setting relay opposite to next scheduled event...");

  DateTime now = getCurrentDateTime();
  uint8_t currentDay = now.dayOfTheWeek();
  uint8_t currentHour = now.hour();
  uint8_t currentMinute = now.minute();
  int nextIdx = -1;

  // Scan the coming 7 days (wrap-around)
  for (int dOffset = 0; dOffset < 7 && nextIdx < 0; dOffset++) {
    uint8_t dayToCheck = (currentDay + dOffset) % 7;
    for (int i = 0; i < scheduleCount; i++) {
      const auto &e = schedule[i];
      if (e.day != dayToCheck) continue;
      // On the same day, ignore times that already passed.
      if (dOffset == 0 && (e.hour < currentHour || (e.hour == currentHour && e.minute <= currentMinute))) continue;
      if (nextIdx < 0 ||
          e.day < schedule[nextIdx].day ||
          (e.day == schedule[nextIdx].day && e.hour < schedule[nextIdx].hour) ||
          (e.day == schedule[nextIdx].day && e.hour == schedule[nextIdx].hour && e.minute < schedule[nextIdx].minute)) {
        nextIdx = i;
      }
    }
  }
  if (nextIdx >= 0) {
    setLocalRelayState(!schedule[nextIdx].state);
    Serial.printf("Relay set opposite to event at %02d:%02d on day %d\n",
                  schedule[nextIdx].hour, schedule[nextIdx].minute, schedule[nextIdx].day);
  }
}

// At boot, apply the last event that occurred (searching backward up to 7 days)
// so the device resumes in the correct state even after power loss.
void setRelayToLastEvent() {
  if (!timeValid || scheduleCount == 0) return;
  Serial.println("Setting relay to last event state...");

  DateTime now = getCurrentDateTime();
  uint8_t currentDay = now.dayOfTheWeek();
  uint8_t currentHour = now.hour();
  uint8_t currentMinute = now.minute();
  int lastIdx = -1;

  for (int dOffset = 0; dOffset < 7 && lastIdx < 0; dOffset++) {
    int8_t dayToCheck = (currentDay - dOffset + 7) % 7;
    for (int i = 0; i < scheduleCount; i++) {
      const auto &e = schedule[i];
      if (e.day != dayToCheck) continue;
      // On the same day, only consider times that already happened.
      if (dOffset == 0 && (e.hour > currentHour || (e.hour == currentHour && e.minute > currentMinute))) continue;
      if (lastIdx < 0 ||
          e.day > schedule[lastIdx].day ||
          (e.day == schedule[lastIdx].day && e.hour > schedule[lastIdx].hour) ||
          (e.day == schedule[lastIdx].day && e.hour == schedule[lastIdx].hour && e.minute > schedule[lastIdx].minute)) {
        lastIdx = i;
      }
    }
  }
  if (lastIdx >= 0) {
    setLocalRelayState(schedule[lastIdx].state);
    Serial.printf("Relay set to last event at %02d:%02d on day %d\n",
                  schedule[lastIdx].hour, schedule[lastIdx].minute, schedule[lastIdx].day);
  }
}

