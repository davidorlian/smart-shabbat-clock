#include "control_actions.h"
#include "hc12_comm.h"
#include "time_utils.h"
#include "web_api.h"

extern bool shabbatMode;
extern bool timeValid;
extern uint8_t relayMode;

void setLocalRelayState(bool);

ActionResult makeActionResult(bool ok, const String& code, const String& message) {
  ActionResult result{ok, code, message};
  return result;
}

ActionResult applyRelayModeAction(const String& mode) {
  if (mode == "on") {
    relayMode = 1;
    saveRelayMode(relayMode);
    setLocalRelayState(true);
    return makeActionResult(true, "applied", "relay mode set to on");
  }

  if (mode == "off") {
    relayMode = 0;
    saveRelayMode(relayMode);
    setLocalRelayState(false);
    return makeActionResult(true, "applied", "relay mode set to off");
  }

  if (mode == "auto") {
    relayMode = 2;
    saveRelayMode(relayMode);
    if (timeValid) setRelayToLastEvent();
    return makeActionResult(true, "applied", "relay mode set to auto");
  }

  return makeActionResult(false, "invalid_relay_mode", "unsupported relay mode");
}

ActionResult applyShabbatModeAction(const String& mode) {
  if (mode != "shabbat" && mode != "week") {
    return makeActionResult(false, "invalid_shabbat_mode", "unsupported shabbat mode");
  }

  if (!sendHC12AndWaitAck(mode)) {
    return makeActionResult(false, "hc12_no_ack", "HC-12 no ACK");
  }

  shabbatMode = (mode == "shabbat");
  saveShabbatMode(shabbatMode);
  return makeActionResult(true, "applied", "shabbat mode set to " + mode);
}

ActionResult replaceScheduleAction(uint32_t baseScheduleRevision,
                                   const ScheduleEntry entries[],
                                   uint8_t entryCount) {
  if (baseScheduleRevision != scheduleRevision) {
    return makeActionResult(false, "stale_schedule",
                            "schedule revision does not match device state");
  }

  if (entryCount > MAX_EVENTS) {
    return makeActionResult(false, "schedule_full", "schedule has too many events");
  }

  for (uint8_t i = 0; i < entryCount; i++) {
    if (entries[i].day > 6 || entries[i].hour > 23 || entries[i].minute > 59) {
      return makeActionResult(false, "invalid_schedule", "schedule event has invalid values");
    }
  }

  scheduleCount = entryCount;
  for (uint8_t i = 0; i < entryCount; i++) {
    schedule[i] = entries[i];
  }

  sortSchedule();
  normalizeSchedule();
  saveSchedule();

  if (relayMode == 2 && timeValid) {
    setRelayToLastEvent();
  }

  return makeActionResult(true, "applied", "schedule replaced");
}
