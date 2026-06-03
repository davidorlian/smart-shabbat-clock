#ifndef CONTROL_ACTIONS_H
#define CONTROL_ACTIONS_H

#include "schedule.h"
#include <Arduino.h>
#include <stdint.h>

struct ActionResult {
  bool ok;
  String code;
  String message;
};

ActionResult makeActionResult(bool ok, const String& code, const String& message);
ActionResult applyRelayModeAction(const String& mode);
ActionResult applyShabbatModeAction(const String& mode);
ActionResult replaceScheduleAction(uint32_t baseScheduleRevision,
                                   const ScheduleEntry entries[],
                                   uint8_t entryCount);

#endif // CONTROL_ACTIONS_H
