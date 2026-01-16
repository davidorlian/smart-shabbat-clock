#include "peripherals.h"
#include "time_utils.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <RTClib.h>

// Hardware detection (LCD/RTC), LCD updates, and Wi-Fi reconnects.
extern const char* ssid;
extern const char* password;
extern bool shabbatMode;
extern bool relay_state;
extern bool timeValid;

DateTime getCurrentDateTime();

// CHANGE HERE: WiFi retry interval (ms).
static const unsigned long WIFI_RETRY_INTERVAL = 60000; // ms

unsigned long lastWiFiAttempt = 0;
static byte wifiIcon[8] = {
  B00000,
  B01110,
  B10001,
  B00100,
  B01010,
  B00000,
  B00100,
  B00000
};

// Safe I2C probe so the code can run without soldered peripherals.
bool isI2CDeviceConnected(uint8_t address) {
  Wire.beginTransmission(address);
  return (Wire.endTransmission() == 0);
}

// Initialize LCD and RTC only if they are physically connected
void initPeripheralsSafely() {
  Wire.begin(SDA_PIN, SCL_PIN);

  // LCD (optional) - CHANGE HERE: I2C address if needed.
  if (isI2CDeviceConnected(0x27)) {
    lcd.init();
    lcd.createChar(0, wifiIcon);
    lcd.backlight();
    lcd.clear();
    lcd.print("Smart Shabbat");
    lcd.setCursor(0, 1);
    lcd.print("Clock Init...");
    lcdAvailable = true;
    Serial.println("LCD connected.");
  }
  
  // RTC (optional) - CHANGE HERE: I2C address if needed.
  if (isI2CDeviceConnected(0x68)) {
    rtcAvailable = rtc.begin();
    Serial.println("RTC connected.");
  }
}

// Initiate WiFi connection
void connectToWiFi() {
  if (lcdAvailable) {
    lcd.clear();
    lcd.print("WiFi: Trying");
  }
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to WiFi SSID: %s...\n", ssid);
}

// Check connection status and retry if needed (non-blocking)
void handleWiFiReconnect() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWiFiAttempt < WIFI_RETRY_INTERVAL) return;
  Serial.println("WiFi not connected. Retrying...");
  connectToWiFi();
  lastWiFiAttempt = millis();
}

// Update LCD content only when changed
void updateDisplay() {
  if (!lcdAvailable) return;

  DateTime now = getCurrentDateTime();
  const char* dayNames[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};

  static String lastDisplayLine1 = "";
  static String lastDisplayLine2 = "";

  // CHANGE HERE: edit the text shown on the LCD.
  char lineBuffer[17];
  snprintf(lineBuffer, sizeof(lineBuffer), "%s %02d:%02d",
           dayNames[now.dayOfTheWeek()], now.hour(), now.minute());
  String line1 = lineBuffer;
  String line2 = (shabbatMode ? "Shabbat " : "Week    ");
  line2 += (relay_state ? "ON" : "OFF");

  if (line1 != lastDisplayLine1) {
    lcd.setCursor(0, 0);
    lcd.print("                ");
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lastDisplayLine1 = line1;
  }

  // Wi-Fi indicator (custom char 0) at top right corner.
  lcd.setCursor(15, 0);
  if (WiFi.status() == WL_CONNECTED) lcd.write((byte)0);
  else lcd.print("X");

  if (line2 != lastDisplayLine2) {
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print(line2);
    lastDisplayLine2 = line2;
  }
}
