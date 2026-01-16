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


// ---------------------- Setup ----------------------
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("\nBooting Smart Shabbat Clock...");

  // 1. Setup GPIOs
  pinMode(RELAY_LOCAL, OUTPUT);
  setLocalRelayState(false); // deterministic startup state
  pinMode(BUTTON_OVERRIDE, INPUT_PULLUP);

  // Optional peripherals (auto-detect)
  Serial.println("Initializing I2C peripherals...");
  initPeripheralsSafely();
  Serial.print("LCD Available: ");
  Serial.println(lcdAvailable ? "YES" : "NO");
  Serial.print("RTC Available: ");
  Serial.println(rtcAvailable ? "YES" : "NO");

  // 3. Setup Radio
  HC12.begin(9600, SERIAL_8N1, HC12_RX, HC12_TX);

  // 4. Connect WiFi
  connectToWiFi();
  lastWiFiAttempt = millis();
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts++ < 50) {
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed.");
  }

  // 5. Setup OTA
  ArduinoOTA.setHostname("ShabbatClock");
  ArduinoOTA.begin();
  Serial.println("OTA ready.");

  // 6. Sync Time
  syncTimeAtBoot();

  // On a new firmware build, wipe schedule.
  if (checkIfNewFlash()) {
    Serial.println("First boot");
    clearScheduleStorage();
  } else {
    loadSchedule();
  }

  // 7. Restore State
  setRelayToLastEvent();

  // 8. Start Web Server
  initWebServer(); 
  server.begin();
  Serial.println("Web server started.");
  Serial.println("Setup complete. System is ready.\n");
  lcd.clear();
  lcd.print(WiFi.localIP());
  delay(1500);
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
