#include "cloud_sync.h"

#include "control_actions.h"
#include "schedule.h"
#include "time_utils.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#ifndef __has_include
#define __has_include(x) 0
#endif

#if __has_include("firebase_config.h")
#include "firebase_config.h"
#define CLOUD_SYNC_CONFIGURED 1
#else
#define CLOUD_SYNC_CONFIGURED 0
#endif

#if CLOUD_SYNC_CONFIGURED

#ifndef FIREBASE_API_KEY
#error "Missing FIREBASE_API_KEY in firebase_config.h"
#endif
#ifndef FIREBASE_DATABASE_URL
#error "Missing FIREBASE_DATABASE_URL in firebase_config.h"
#endif
#ifndef FIREBASE_DEVICE_EMAIL
#error "Missing FIREBASE_DEVICE_EMAIL in firebase_config.h"
#endif
#ifndef FIREBASE_DEVICE_PASSWORD
#error "Missing FIREBASE_DEVICE_PASSWORD in firebase_config.h"
#endif
#ifndef FIREBASE_DEVICE_ID
#error "Missing FIREBASE_DEVICE_ID in firebase_config.h"
#endif
#ifndef FIREBASE_ALLOW_INSECURE_TLS
#define FIREBASE_ALLOW_INSECURE_TLS 1
#endif

extern Preferences prefs;
extern bool relay_state;
extern bool shabbatMode;
extern bool timeValid;
extern bool hc12Ok;
extern uint8_t relayMode;

static const unsigned long AUTH_RETRY_INTERVAL = 60000;
static const unsigned long COMMAND_POLL_INTERVAL = 10000;
static const unsigned long STATUS_HEARTBEAT_INTERVAL = 60000;
static const unsigned long STATUS_CHANGE_MIN_INTERVAL = 5000;
static const uint8_t MAX_COMMAND_BATCH = 5;

struct CloudCommand {
  String id;
  uint32_t seq;
  String type;
  String mode;
  uint32_t baseScheduleRevision;
  ScheduleEntry events[MAX_EVENTS];
  uint8_t eventCount;
};

static String idToken;
static unsigned long tokenExpiresAtMs = 0;
static unsigned long lastAuthAttemptMs = 0;
static unsigned long lastCommandPollMs = 0;
static unsigned long lastStatusPublishMs = 0;
static String lastStatusSignature;
static uint32_t lastPublishedScheduleRevision = 0xffffffffUL;
static uint32_t lastProcessedSeq = 0;
static String inFlightCommandId;
static uint32_t inFlightSeq = 0;
static bool pendingBootRecoveryAck = false;
static bool forceStatusPublish = true;

static String jsonEscape(const String& value) {
  String out;
  out.reserve(value.length() + 8);
  for (uint16_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else {
      out += c;
    }
  }
  return out;
}

static String jsonString(const String& value) {
  return "\"" + jsonEscape(value) + "\"";
}

static String jsonBool(bool value) {
  return value ? "true" : "false";
}

static void configureClient(WiFiClientSecure& client) {
#if FIREBASE_ALLOW_INSECURE_TLS
  client.setInsecure();
#endif
}

static bool httpRequest(const String& method,
                        const String& url,
                        const String& body,
                        int& statusCode,
                        String& response) {
  WiFiClientSecure client;
  configureClient(client);

  HTTPClient http;
  http.setTimeout(6000);
  if (!http.begin(client, url)) {
    statusCode = -1;
    response = "http.begin failed";
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  if (method == "GET") statusCode = http.GET();
  else if (method == "POST") statusCode = http.POST(body);
  else if (method == "PUT") statusCode = http.PUT(body);
  else {
    statusCode = -2;
    response = "unsupported method";
    http.end();
    return false;
  }

  response = http.getString();
  http.end();
  return statusCode >= 200 && statusCode < 300;
}

static String databaseBaseUrl() {
  String base = FIREBASE_DATABASE_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base;
}

static String databaseUrl(const String& path, const String& query = "") {
  String cleanPath = path;
  while (cleanPath.startsWith("/")) cleanPath.remove(0, 1);

  String url = databaseBaseUrl() + "/" + cleanPath + ".json";
  if (query.length() > 0) {
    url += "?" + query + "&auth=" + idToken;
  } else {
    url += "?auth=" + idToken;
  }
  return url;
}

static bool parseJsonStringAt(const String& json, int quotePos, String& value, int& nextPos) {
  if (quotePos < 0 || quotePos >= json.length() || json[quotePos] != '"') return false;

  value = "";
  bool escaped = false;
  for (int i = quotePos + 1; i < json.length(); i++) {
    char c = json[i];
    if (escaped) {
      value += c;
      escaped = false;
    } else if (c == '\\') {
      escaped = true;
    } else if (c == '"') {
      nextPos = i + 1;
      return true;
    } else {
      value += c;
    }
  }
  return false;
}

static bool findStringValue(const String& json, const String& key, String& value) {
  int keyPos = json.indexOf("\"" + key + "\"");
  if (keyPos < 0) return false;

  int colon = json.indexOf(':', keyPos);
  if (colon < 0) return false;

  int quote = json.indexOf('"', colon + 1);
  int next = 0;
  return parseJsonStringAt(json, quote, value, next);
}

static bool findNumberValue(const String& json, const String& key, uint32_t& value) {
  int keyPos = json.indexOf("\"" + key + "\"");
  if (keyPos < 0) return false;

  int colon = json.indexOf(':', keyPos);
  if (colon < 0) return false;

  int pos = colon + 1;
  while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r')) pos++;

  uint32_t parsed = 0;
  bool foundDigit = false;
  while (pos < json.length() && json[pos] >= '0' && json[pos] <= '9') {
    foundDigit = true;
    parsed = parsed * 10 + (json[pos] - '0');
    pos++;
  }

  if (!foundDigit) return false;
  value = parsed;
  return true;
}

static bool extractValueBlock(const String& json, const String& key, String& value) {
  int keyPos = json.indexOf("\"" + key + "\"");
  if (keyPos < 0) return false;

  int colon = json.indexOf(':', keyPos);
  if (colon < 0) return false;

  int pos = colon + 1;
  while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r')) pos++;
  if (pos >= json.length()) return false;

  char open = json[pos];
  char close = (open == '{') ? '}' : ((open == '[') ? ']' : '\0');
  if (close == '\0') return false;

  int depth = 0;
  bool inString = false;
  bool escaped = false;
  for (int i = pos; i < json.length(); i++) {
    char c = json[i];
    if (inString) {
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '"') inString = false;
      continue;
    }

    if (c == '"') inString = true;
    else if (c == open) depth++;
    else if (c == close) {
      depth--;
      if (depth == 0) {
        value = json.substring(pos, i + 1);
        return true;
      }
    }
  }

  return false;
}

static bool parseEventObject(const String& object, ScheduleEntry& event) {
  uint32_t day = 0;
  uint32_t hour = 0;
  uint32_t minute = 0;
  String state;

  if (!findNumberValue(object, "day", day)) return false;
  if (!findNumberValue(object, "hour", hour)) return false;
  if (!findNumberValue(object, "minute", minute)) return false;
  if (!findStringValue(object, "state", state)) return false;
  if (day > 6 || hour > 23 || minute > 59) return false;
  if (state != "on" && state != "off") return false;

  event.day = (uint8_t)day;
  event.hour = (uint8_t)hour;
  event.minute = (uint8_t)minute;
  event.state = (state == "on");
  return true;
}

static bool parseEvents(const String& eventsJson, CloudCommand& command) {
  command.eventCount = 0;

  int pos = 0;
  while (pos < eventsJson.length()) {
    int start = eventsJson.indexOf('{', pos);
    if (start < 0) break;

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    int end = -1;

    for (int i = start; i < eventsJson.length(); i++) {
      char c = eventsJson[i];
      if (inString) {
        if (escaped) escaped = false;
        else if (c == '\\') escaped = true;
        else if (c == '"') inString = false;
        continue;
      }

      if (c == '"') inString = true;
      else if (c == '{') depth++;
      else if (c == '}') {
        depth--;
        if (depth == 0) {
          end = i;
          break;
        }
      }
    }

    if (end < 0) return false;
    if (command.eventCount >= MAX_EVENTS) return false;

    ScheduleEntry event;
    if (!parseEventObject(eventsJson.substring(start, end + 1), event)) return false;
    command.events[command.eventCount++] = event;
    pos = end + 1;
  }

  return true;
}

static bool parseCommandObject(const String& id, const String& object, CloudCommand& command) {
  command = CloudCommand();
  command.id = id;

  if (!findNumberValue(object, "seq", command.seq)) return false;
  if (!findStringValue(object, "type", command.type)) return false;

  if (command.type == "relay_mode" || command.type == "shabbat_mode") {
    return findStringValue(object, "mode", command.mode);
  }

  if (command.type == "replace_schedule") {
    if (!findNumberValue(object, "baseScheduleRevision", command.baseScheduleRevision)) return false;
    String eventsJson;
    if (!extractValueBlock(object, "events", eventsJson)) return false;
    return parseEvents(eventsJson, command);
  }

  return true;
}

static uint8_t parseCommandList(const String& json, CloudCommand commands[], uint8_t maxCommands) {
  if (json == "null" || json.length() < 2) return 0;

  uint8_t count = 0;
  int pos = 0;
  while (pos < json.length() && count < maxCommands) {
    int keyQuote = json.indexOf('"', pos);
    if (keyQuote < 0) break;

    String id;
    int afterKey = 0;
    if (!parseJsonStringAt(json, keyQuote, id, afterKey)) break;

    int colon = json.indexOf(':', afterKey);
    if (colon < 0) break;

    int objectStart = json.indexOf('{', colon + 1);
    if (objectStart < 0) break;

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    int objectEnd = -1;

    for (int i = objectStart; i < json.length(); i++) {
      char c = json[i];
      if (inString) {
        if (escaped) escaped = false;
        else if (c == '\\') escaped = true;
        else if (c == '"') inString = false;
        continue;
      }

      if (c == '"') inString = true;
      else if (c == '{') depth++;
      else if (c == '}') {
        depth--;
        if (depth == 0) {
          objectEnd = i;
          break;
        }
      }
    }

    if (objectEnd < 0) break;

    CloudCommand command;
    if (parseCommandObject(id, json.substring(objectStart, objectEnd + 1), command) &&
        command.seq > lastProcessedSeq) {
      commands[count++] = command;
    }
    pos = objectEnd + 1;
  }

  for (uint8_t i = 0; i < count; i++) {
    for (uint8_t j = i + 1; j < count; j++) {
      if (commands[j].seq < commands[i].seq) {
        CloudCommand tmp = commands[i];
        commands[i] = commands[j];
        commands[j] = tmp;
      }
    }
  }

  return count;
}

static void loadCloudState() {
  prefs.begin("cloud", true);
  lastProcessedSeq = prefs.getUInt("lastSeq", 0);
  inFlightCommandId = prefs.getString("flightId", "");
  inFlightSeq = prefs.getUInt("flightSeq", 0);
  prefs.end();
  pendingBootRecoveryAck = (inFlightCommandId.length() > 0 && inFlightSeq > 0);
}

static void saveLastProcessedSeq(uint32_t seq) {
  lastProcessedSeq = seq;
  prefs.begin("cloud", false);
  prefs.putUInt("lastSeq", lastProcessedSeq);
  prefs.end();
}

static void saveInFlightCommand(const CloudCommand& command) {
  inFlightCommandId = command.id;
  inFlightSeq = command.seq;
  prefs.begin("cloud", false);
  prefs.putString("flightId", inFlightCommandId);
  prefs.putUInt("flightSeq", inFlightSeq);
  prefs.end();
}

static void clearInFlightCommand() {
  inFlightCommandId = "";
  inFlightSeq = 0;
  pendingBootRecoveryAck = false;
  prefs.begin("cloud", false);
  prefs.remove("flightId");
  prefs.remove("flightSeq");
  prefs.end();
}

static bool signInIfNeeded() {
  if (idToken.length() > 0 && millis() < tokenExpiresAtMs) return true;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (lastAuthAttemptMs != 0 && millis() - lastAuthAttemptMs < AUTH_RETRY_INTERVAL) return false;

  lastAuthAttemptMs = millis();

  String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" + String(FIREBASE_API_KEY);
  String body = String("{\"email\":") + jsonString(FIREBASE_DEVICE_EMAIL) +
                ",\"password\":" + jsonString(FIREBASE_DEVICE_PASSWORD) +
                ",\"returnSecureToken\":true}";

  int status = 0;
  String response;
  if (!httpRequest("POST", url, body, status, response)) {
    Serial.printf("Firebase auth failed: HTTP %d\n", status);
    return false;
  }

  String token;
  if (!findStringValue(response, "idToken", token)) {
    Serial.println("Firebase auth failed: missing idToken");
    return false;
  }

  uint32_t expiresIn = 3600;
  findNumberValue(response, "expiresIn", expiresIn);

  idToken = token;
  tokenExpiresAtMs = millis() + ((expiresIn > 120 ? expiresIn - 60 : expiresIn) * 1000UL);
  Serial.println("Firebase auth ready.");
  return true;
}

static String statusSignature() {
  char buf[6] = {0};
  if (timeValid) {
    DateTime now = getCurrentDateTime();
    snprintf(buf, sizeof(buf), "%02d:%02d", now.hour(), now.minute());
  }

  return String(relay_state ? "1" : "0") + "|" +
         String(relayMode) + "|" +
         String(shabbatMode ? "1" : "0") + "|" +
         String(buf) + "|" +
         String(timeValid ? "1" : "0") + "|" +
         String(hc12Ok ? "1" : "0") + "|" +
         String(lastProcessedSeq) + "|" +
         String(scheduleRevision);
}

static String statusJson() {
  char buf[6] = {0};
  if (timeValid) {
    DateTime now = getCurrentDateTime();
    snprintf(buf, sizeof(buf), "%02d:%02d", now.hour(), now.minute());
  }

  return String("{") +
         "\"relay\":" + jsonBool(relay_state) +
         ",\"relayMode\":" + String(relayMode) +
         ",\"shabbat\":" + jsonBool(shabbatMode) +
         ",\"time\":\"" + String(buf) + "\"" +
         ",\"timeValid\":" + jsonBool(timeValid) +
         ",\"hc12Ok\":" + jsonBool(hc12Ok) +
         ",\"lastSeen\":{\".sv\":\"timestamp\"}" +
         ",\"lastProcessedSeq\":" + String(lastProcessedSeq) +
         ",\"scheduleRevision\":" + String(scheduleRevision) +
         "}";
}

static String scheduleJson() {
  String json = String("{\"revision\":") + String(scheduleRevision) + ",\"events\":[";
  for (uint8_t i = 0; i < scheduleCount; i++) {
    if (i > 0) json += ",";
    json += "{\"day\":" + String(schedule[i].day) +
            ",\"hour\":" + String(schedule[i].hour) +
            ",\"minute\":" + String(schedule[i].minute) +
            ",\"state\":\"" + String(schedule[i].state ? "on" : "off") + "\"}";
  }
  json += "]}";
  return json;
}

static bool putDatabaseJson(const String& path, const String& body) {
  int status = 0;
  String response;
  bool ok = httpRequest("PUT", databaseUrl(path), body, status, response);
  if (!ok) {
    Serial.printf("Firebase PUT failed %s: HTTP %d\n", path.c_str(), status);
  }
  return ok;
}

static void publishStatusIfDue(bool force) {
  String signature = statusSignature();
  unsigned long nowMs = millis();
  bool changed = signature != lastStatusSignature;
  bool heartbeatDue = lastStatusPublishMs == 0 ||
                      nowMs - lastStatusPublishMs >= STATUS_HEARTBEAT_INTERVAL;
  bool changeDue = changed &&
                   (lastStatusPublishMs == 0 ||
                    nowMs - lastStatusPublishMs >= STATUS_CHANGE_MIN_INTERVAL);

  if (!force && !heartbeatDue && !changeDue) return;

  if (putDatabaseJson(String("devices/") + FIREBASE_DEVICE_ID + "/state/status", statusJson())) {
    lastStatusSignature = signature;
    lastStatusPublishMs = nowMs;
    forceStatusPublish = false;
  }
}

static void publishScheduleIfNeeded() {
  if (scheduleRevision == lastPublishedScheduleRevision) return;

  if (putDatabaseJson(String("devices/") + FIREBASE_DEVICE_ID + "/state/schedule", scheduleJson())) {
    lastPublishedScheduleRevision = scheduleRevision;
  }
}

static bool writeAckFor(const String& commandId,
                        uint32_t seq,
                        const ActionResult& result) {
  String body = String("{\"seq\":") + String(seq) +
                ",\"ok\":" + jsonBool(result.ok) +
                ",\"code\":" + jsonString(result.code) +
                ",\"message\":" + jsonString(result.message) +
                ",\"ackedAt\":{\".sv\":\"timestamp\"}}";

  return putDatabaseJson(String("devices/") + FIREBASE_DEVICE_ID + "/acks/" + commandId, body);
}

static ActionResult executeCommand(const CloudCommand& command) {
  if (command.type == "relay_mode") {
    return applyRelayModeAction(command.mode);
  }

  if (command.type == "shabbat_mode") {
    return applyShabbatModeAction(command.mode);
  }

  if (command.type == "replace_schedule") {
    return replaceScheduleAction(command.baseScheduleRevision, command.events, command.eventCount);
  }

  return makeActionResult(false, "unsupported_command", "unsupported command type");
}

static void pollOneCommand() {
  if (lastCommandPollMs != 0 && millis() - lastCommandPollMs < COMMAND_POLL_INTERVAL) return;
  lastCommandPollMs = millis();
  if (pendingBootRecoveryAck) return;

  String query = "orderBy=%22seq%22&startAt=" + String(lastProcessedSeq + 1) +
                 "&limitToFirst=" + String(MAX_COMMAND_BATCH);
  String path = String("devices/") + FIREBASE_DEVICE_ID + "/commands";

  int status = 0;
  String response;
  if (!httpRequest("GET", databaseUrl(path, query), "", status, response)) {
    Serial.printf("Firebase command poll failed: HTTP %d\n", status);
    return;
  }

  CloudCommand commands[MAX_COMMAND_BATCH];
  uint8_t count = parseCommandList(response, commands, MAX_COMMAND_BATCH);
  if (count == 0) return;

  CloudCommand command = commands[0];
  Serial.printf("Firebase command %s seq %lu type %s\n",
                command.id.c_str(),
                (unsigned long)command.seq,
                command.type.c_str());

  saveInFlightCommand(command);
  ActionResult result = executeCommand(command);
  if (writeAckFor(command.id, command.seq, result)) {
    saveLastProcessedSeq(command.seq);
    clearInFlightCommand();
    forceStatusPublish = true;
  } else {
    Serial.println("Firebase ACK write failed; keeping in-flight marker.");
  }
}

static void recoverInFlightAfterBoot() {
  if (!pendingBootRecoveryAck) return;

  ActionResult result = makeActionResult(false,
                                         "unknown_after_reboot",
                                         "device rebooted before command completion could be confirmed");
  if (writeAckFor(inFlightCommandId, inFlightSeq, result)) {
    saveLastProcessedSeq(inFlightSeq);
    clearInFlightCommand();
    forceStatusPublish = true;
    Serial.println("Firebase in-flight command marked unknown after reboot.");
  }
}

#endif // CLOUD_SYNC_CONFIGURED

void initCloudSync() {
#if CLOUD_SYNC_CONFIGURED
  loadCloudState();
  Serial.printf("Cloud sync enabled for device %s. Last seq: %lu\n",
                FIREBASE_DEVICE_ID,
                (unsigned long)lastProcessedSeq);
  if (pendingBootRecoveryAck) {
    Serial.printf("Firebase in-flight command pending recovery: %s seq %lu\n",
                  inFlightCommandId.c_str(),
                  (unsigned long)inFlightSeq);
  }
#else
  Serial.println("Cloud sync disabled: missing firebase_config.h");
#endif
}

void tickCloudSync() {
#if CLOUD_SYNC_CONFIGURED
  if (WiFi.status() != WL_CONNECTED) return;
  if (!signInIfNeeded()) return;

  recoverInFlightAfterBoot();
  pollOneCommand();
  publishScheduleIfNeeded();
  publishStatusIfDue(forceStatusPublish);
#endif
}
