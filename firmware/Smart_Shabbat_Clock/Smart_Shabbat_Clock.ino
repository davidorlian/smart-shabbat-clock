// Smart Shabbat Clock - Main Controller
// ESP32-based: web UI, weekly schedule in NVS, NTP/RTC time sync, relay modes.

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <time.h>
#include <RTClib.h>
#include <HardwareSerial.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

#include "index_page.h"
#include "schedule.h"
#include "web_api.h"
#include "peripherals.h"
#include "time_utils.h"

// ---------------------- WiFi Settings ----------------------
// CHANGE HERE: WiFi name/password + timezone offset (seconds).
const char* ssid     = "YOUR_SSID_HERE";
const char* password = "YOUR_PASSWORD_HERE";
const long gmtOffset_sec = 3 * 3600;
struct tm timeinfo;  // used by getLocalTime()

// ---------------------- Hardware Pins ----------------------
// CHANGE HERE: pin mapping for your board.
const uint8_t SDA_PIN         = 6;
const uint8_t SCL_PIN         = 7;
const uint8_t RELAY_LOCAL     = 3;
const uint8_t HC12_RX         = 5;
const uint8_t HC12_TX         = 4;
const uint8_t BUTTON_OVERRIDE = 18;

// ---------------------- Peripherals ----------------------
// CHANGE HERE: LCD I2C address (size comes from peripherals_copy.h).
LiquidCrystal_I2C lcd(0x27, LCD_COLS, LCD_ROWS);
RTC_DS3231 rtc;
HardwareSerial HC12(1);
WebServer server(80);
Preferences prefs;
bool firstBootAfterFlash = false; // set by checkIfNewFlash()

// ---------------------- Global Variables ----------------------
// relayMode: 0=force OFF, 1=force ON, 2=AUTO (follow schedule).
bool relay_state = false;
bool shabbatMode = false;
uint8_t relayMode = 2;  // default AUTO

bool lcdAvailable = false;
bool rtcAvailable = false;

// Connection/Time-keeping cadences.
unsigned long lastButtonPress = 0; // debounce for override button

// New globals
bool timeValid = false;     // true when system time is trustworthy
bool hc12Ok    = false;     // updated after last HC-12 command/handshake

// Helper to toggle the physical relay and update global state
void setLocalRelayState(bool newState) {
  if (relay_state == newState) return;            
  digitalWrite(RELAY_LOCAL, newState ? HIGH : LOW);  
  relay_state = newState;                            
  Serial.printf("Relay state changed to %s\n", newState ? "ON" : "OFF");
  // Persist relay state so power loss doesn't reset it
  prefs.begin("state", false);
  prefs.putBool("relay", relay_state);
  prefs.end();
}

// Detects a "new build" by comparing __DATE__/__TIME__ with last saved value.
// Used to decide whether to wipe schedule on first boot after flashing.
bool checkIfNewFlash() {
    String currBuild = String(__DATE__) + " " + String(__TIME__);
    prefs.begin("fw", false);
    String prevBuild = prefs.getString("fw_build", "");
    bool isNew = (prevBuild != currBuild);
    if (isNew) { prefs.putString("fw_build", currBuild); }
    prefs.end();
    return isNew;
}

// Persisted mode helpers (save/load only)
void saveRelayMode(uint8_t mode) {
  prefs.begin("state", false);
  prefs.putUChar("relayMode", mode);
  prefs.end();
}

void saveShabbatMode(bool mode) {
  prefs.begin("state", false);
  prefs.putBool("shabbat", mode);
  prefs.end();
}

void loadPersistedModes() {
  prefs.begin("state", true);
  relayMode = prefs.getUChar("relayMode", relayMode);
  shabbatMode = prefs.getBool("shabbat", shabbatMode);
  prefs.end();
}


// ---------------------- Setup ----------------------
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("\nBooting Smart Shabbat Clock...");

  // 1) Start from a SAFE physical state (relay OFF).
  pinMode(RELAY_LOCAL, OUTPUT);
  digitalWrite(RELAY_LOCAL, LOW);
  pinMode(BUTTON_OVERRIDE, INPUT_PULLUP);

  // 2) Load persisted user modes from NVS.
  loadPersistedModes();

  // 3) Init optional peripherals (LCD/RTC auto-detect).
  Serial.println("Initializing I2C peripherals...");
  initPeripheralsSafely();
  Serial.printf("LCD Available: %s\n", lcdAvailable ? "YES" : "NO");
  Serial.printf("RTC Available: %s\n", rtcAvailable ? "YES" : "NO");

  // 4) Decide whether RTC time is trustworthy.
  // If RTC lost power, time stays invalid until NTP/manual set fixes it.
  if (rtcAvailable && !rtc.lostPower()) timeValid = true;
  else timeValid = false;

  // 5) Init HC-12 radio UART.
  HC12.begin(9600, SERIAL_8N1, HC12_RX, HC12_TX);

  // 6) Start Wi-Fi connection (non-blocking at boot).
  connectToWiFi();
  lastWiFiAttempt = millis();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected quickly. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi not connected yet (will retry in background).");
  }

  // 7) Init OTA.
  ArduinoOTA.setHostname("ShabbatClock");
  ArduinoOTA.begin();
  Serial.println("OTA ready.");

  // 8) Attempt time sync at boot.
  syncTimeAtBoot();

  // 9) On a new firmware build, wipe schedule; otherwise load it.
  if (checkIfNewFlash()) {
    Serial.println("First boot");
    clearScheduleStorage();
  } else {
    loadSchedule();
  }

  // 10) Restore relay state from authoritative mode logic.
  if (relayMode == 0) {
    setLocalRelayState(false);      // manual force OFF
  } else if (relayMode == 1) {
    setLocalRelayState(true);       // manual force ON
  } else if (timeValid) {           // AUTO
    setRelayToLastEvent();
  } else {
    setLocalRelayState(false);      // AUTO but time invalid => stay safe OFF
  }

  // 11) Start web server.
  initWebServer(); 
  server.begin();
  Serial.println("Web server started.");
  Serial.println("Setup complete. System is ready.\n");
  if (lcdAvailable) {
    lcd.clear();
    lcd.print(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("No WiFi"));
    delay(300);
  }
}

// ---------------------- Loop ----------------------
// Main loop is lightweight: handle OTA, HTTP, LCD, schedule,
// Wi-Fi keepalive, manual override, and periodic NTP to RTC resync.
void loop() {
  static unsigned long lastDisplayUpdate = 0;

  // Handle background tasks
  ArduinoOTA.handle();
  server.handleClient();

  // Update display only every 500ms to avoid I2C congestion, but don't block the loop
  if (millis() - lastDisplayUpdate > 500) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  // Apply schedule logic if in AUTO mode and time is valid
  if (timeValid) applyScheduleLogic();
  // Check WiFi connection periodically
  handleWiFiReconnect();

  // Manual toggle with debounce (active-low button).
  if (digitalRead(BUTTON_OVERRIDE) == LOW && millis() - lastButtonPress > 300) {
    setLocalRelayState(!relay_state);
    lastButtonPress = millis();
    Serial.printf("Manual override: Relay %s\n", relay_state ? "ON" : "OFF");
  }

  // Periodically sync NTP to RTC
  tickTimeSync();
}
