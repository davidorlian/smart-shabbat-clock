#include "web_api.h"
#include "schedule.h"
#include "hc12_comm.h"
#include "index_page.h"
#include "time_utils.h"
#include <RTClib.h>
#include <WebServer.h>
#include <time.h>

extern WebServer server;
extern bool relay_state, shabbatMode, timeValid, hc12Ok;
extern uint8_t relayMode;
extern struct tm timeinfo;
extern bool rtcAvailable;
void setLocalRelayState(bool);
void setRelayOppositeToNextEvent();
void setRelayToLastEvent();
void saveSchedule();
void normalizeSchedule();
void sortSchedule();
extern ScheduleEntry schedule[];
extern uint8_t scheduleCount;

// ---------------------- Route Handlers ----------------------

// Serve the main HTML page
void handleRoot() { server.send(200, "text/html", index_html); }

// Return system status as JSON (for UI polling)
void handleStatus() {
    char buf[6] = {0};
    if (timeValid) {
      DateTime now = getCurrentDateTime();
      sprintf(buf, "%02d:%02d", now.hour(), now.minute());
    }

    String json = String("{") +
      "\"relay\":"     + String(relay_state ? "true" : "false") +
      ",\"shabbat\":"  + String(shabbatMode ? "true" : "false") +
      ",\"relayMode\":"+ String(relayMode) +
      ",\"time\":\""   + String(buf) + "\"" +        
      ",\"timeValid\":"+ String(timeValid ? "true" : "false") +
      ",\"hc12Ok\":"   + String(hc12Ok ? "true" : "false") +
    "}";

    server.send(200, "application/json", json);
}

// Handle commands: relay_on, relay_off, relay_auto, shabbat, week
void handleCommand() {
    if (!server.hasArg("c")) { server.send(400, "text/plain", "Missing cmd"); return; }
    String c = server.arg("c");

    if (c == "relay_on")  { relayMode = 1; setLocalRelayState(true);  server.send(204,"text/plain",""); return; }
    if (c == "relay_off") { relayMode = 0; setLocalRelayState(false); server.send(204,"text/plain",""); return; }
    if (c == "relay_auto"){ relayMode = 2; if (timeValid) setRelayOppositeToNextEvent(); server.send(204,"text/plain",""); return; }

    if (c == "shabbat") {
      if (sendHC12AndWaitAck("shabbat")) {
        shabbatMode = true;
        server.send(204, "text/plain", "");
      } else {
        server.send(504, "text/plain", "HC-12 no ACK");
      }
      return;
    }

    if (c == "week") {
      if (sendHC12AndWaitAck("week")) {
        shabbatMode = false;
        server.send(204, "text/plain", "");
      } else {
        server.send(504, "text/plain", "HC-12 no ACK");
      }
      return;
    }

    server.send(400, "text/plain", "Unsupported command");
}

// Add or update a schedule event
void handleScheduleUpdate() {
    if (!server.hasArg("hour") || !server.hasArg("minute") || !server.hasArg("state") || !server.hasArg("day")) {
      server.send(400, "Missing args");
      return;
    }

    if (!timeValid) {
      server.send(409, "text/plain", "Time invalid - scheduling disabled");
      return;
    }

    uint8_t hour = server.arg("hour").toInt();
    uint8_t minute = server.arg("minute").toInt();
    bool state = (server.arg("state") == "on");
    uint8_t day = server.arg("day").toInt();
    if (hour > 23 || minute > 59 || day > 6) {
      server.send(400, "Invalid values");
      return;
    }

    // If an event already exists at the same (day,time):
    for (int i = 0; i < scheduleCount; i++) {
      if (schedule[i].day == day && schedule[i].hour == hour && schedule[i].minute == minute) {

        // Case 2: Same state — nothing to change
        if (schedule[i].state == state) {
          server.send(409, "No change - identical event already exists");
          return;
        }

        // Case 1: Different state — update existing entry
        schedule[i].state = state;
        sortSchedule();
        normalizeSchedule(); // keep only transitions
        saveSchedule();
        server.send(200, "Schedule updated");
        return;
      }
    }

    // Otherwise add new entry (if there is capacity).
    if (scheduleCount >= MAX_EVENTS) {
      server.send(400, "Schedule full");
      return;
    }

    ScheduleEntry e{hour, minute, state, day};
    schedule[scheduleCount++] = e;
    sortSchedule();
    normalizeSchedule();     // compress consecutive duplicates
    saveSchedule();
    server.send(200, "Schedule added");
}

// Return the full schedule as JSON
void handleScheduleList() {
    String json = "[";
    for (int i = 0; i < scheduleCount; i++) {
      if (i > 0) json += ",";
      json += "{\"hour\":" + String(schedule[i].hour) +
              ",\"minute\":" + String(schedule[i].minute) +
              ",\"state\":\"" + String(schedule[i].state ? "on" : "off") + "\"" +
              ",\"day\":" + String(schedule[i].day) + "}";
    }
    json += "]";
    server.send(200, "application/json", json);
}

// Manually set the system time (if NTP fails)
void handleSetTime() {
    if (!server.hasArg("y") || !server.hasArg("m") || !server.hasArg("d") ||
        !server.hasArg("H") || !server.hasArg("M") || !server.hasArg("S")) {
      server.send(400, "Missing args y,m,d,H,M,S");
      return;
    }
    int y = server.arg("y").toInt();
    int m = server.arg("m").toInt();
    int d = server.arg("d").toInt();
    int H = server.arg("H").toInt();
    int M = server.arg("M").toInt();
    int S = server.arg("S").toInt();
    if (y < 2020 || m < 1 || m > 12 || d < 1 || d > 31 || H < 0 || H > 23 || M < 0 || M > 59 || S < 0 || S > 59) {
      server.send(400, "Invalid values");
      return;
    }

    // set system time
    struct tm t = {};
    t.tm_year = y - 1900;
    t.tm_mon  = m - 1;
    t.tm_mday = d;
    t.tm_hour = H;
    t.tm_min  = M;
    t.tm_sec  = S;
    time_t epoch = mktime(&t);
    struct timeval now = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&now, nullptr);

    if (rtcAvailable) rtc.adjust(DateTime(y, m, d, H, M, S));
    timeValid = true;

    if (relayMode == 2) setRelayToLastEvent();

    server.send(200, "Time set");
}

// Delete a specific event
void handleScheduleDelete() {
    if (!server.hasArg("day") || !server.hasArg("hour") || !server.hasArg("minute")) {
      server.send(400, "Missing args"); return;
    }
    uint8_t day = server.arg("day").toInt();
    uint8_t hour = server.arg("hour").toInt();
    uint8_t minute = server.arg("minute").toInt();
    if (day > 6 || hour > 23 || minute > 59) {
      server.send(400, "Invalid values"); return;
    }
  
    int idx = -1;
    for (int i = 0; i < scheduleCount; i++) {
      if (schedule[i].day == day && schedule[i].hour == hour && schedule[i].minute == minute) {
        idx = i; break;
      }
    }
    if (idx < 0) { server.send(404, "Event not found"); return; }
  
    // Compact array (stable order after removal).
    for (int j = idx + 1; j < scheduleCount; j++) {
      schedule[j - 1] = schedule[j];
    }
    scheduleCount--;
  
    sortSchedule();
    normalizeSchedule();
    saveSchedule();
  
    server.send(200, "Event deleted");
}

// ---------------------- Web Server Init ----------------------

void initWebServer() {
  Serial.println("Starting web server...");
  
  // Root serves the embedded HTML UI.
  server.on("/", handleRoot);

  // Lightweight JSON status for the UI polling.
  server.on("/status", handleStatus);

  // Command endpoint for relay mode + Shabbat/Week broadcast to HC-12.
  server.on("/cmd", handleCommand);

  // Schedule management
  server.on("/schedule", handleScheduleUpdate);
  server.on("/schedule_list", handleScheduleList);
  server.on("/schedule_delete", handleScheduleDelete);
  
  // Manual time set
  server.on("/set_time", handleSetTime);
}
