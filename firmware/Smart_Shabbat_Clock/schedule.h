#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <stdint.h>

// ---------------------- Schedule Storage ----------------------
// We store a flat list of events (time + day + ON/OFF).
// Persistence: ESP32 NVS (Preferences) namespace "sched".
// CHANGE HERE: max number of schedule events stored.
#define MAX_EVENTS 32

struct ScheduleEntry {
  uint8_t hour;    // 0...23
  uint8_t minute;  // 0...59
  bool    state;   // true = ON, false = OFF
  uint8_t day;     // 0=Sun ... 6=Sat (matches tm_wday convention except 0=Sun)
};
extern ScheduleEntry schedule[MAX_EVENTS];
extern uint8_t scheduleCount;

void saveSchedule();
void loadSchedule();
void clearScheduleStorage();
void sortSchedule();
void normalizeSchedule();
void applyScheduleLogic();
void setRelayOppositeToNextEvent();
void setRelayToLastEvent();

#endif // SCHEDULE_H
