#include "peripherals.h"
#include "time_utils.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <RTClib.h>

// ---------------------- Externals ----------------------
extern const char* ssid;
extern const char* password;
extern bool shabbatMode;
extern bool relay_state;
extern bool timeValid;

DateTime getCurrentDateTime();

// ---------------------- Wi-Fi ----------------------
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

// ---------------------- LCD Pages ----------------------
// CHANGE HERE: number of pages (update PAGE_INTERVALS to match).
static const uint8_t PAGE_COUNT = 2;
static LcdPage pages[PAGE_COUNT];
static uint8_t currentPage = 0;
// CHANGE HERE: page timing (ms). one interval per page (must match PAGE_COUNT).
static const unsigned long PAGE_INTERVALS[] = { 5000, 5000 };
static unsigned long lastPageSwitch = 0;
static bool pagesInitialized = false;

void LcdPage::init(uint8_t colsIn, uint8_t rowsIn, unsigned long intervalMsIn) {
  cols = colsIn;
  rows = rowsIn;
  intervalMs = intervalMsIn;
  clear();
}

void LcdPage::setLine(uint8_t row, const String& text) {
  if (row >= rows || row >= MAX_LCD_ROWS) return;
  uint8_t allowedCols = cols;
  if (row == 0 && cols > 0) allowedCols = cols - 1; // reserve top-right for icon
  String clipped = text;
  if (clipped.length() > allowedCols) clipped = clipped.substring(0, allowedCols);
  while (clipped.length() < allowedCols) clipped += ' ';
  lines[row] = clipped;
}

void LcdPage::clear() {
  for (uint8_t i = 0; i < MAX_LCD_ROWS; ++i) {
    uint8_t allowedCols = cols;
    if (i == 0 && cols > 0) allowedCols = cols - 1;
    String blank = "";
    for (uint8_t c = 0; c < allowedCols; ++c) blank += ' ';
    lines[i] = blank;
  }
}

void LcdPage::render(LiquidCrystal_I2C& lcdRef) const {
  for (uint8_t r = 0; r < rows && r < MAX_LCD_ROWS; ++r) {
    lcdRef.setCursor(0, r);
    lcdRef.print(lines[r]);
  }
}

void initLcdPages() {
  if (pagesInitialized) return;
  // Change LCD size and page timing above.
  for (uint8_t i = 0; i < PAGE_COUNT; ++i) {
    pages[i].init(LCD_COLS, LCD_ROWS, PAGE_INTERVALS[i]);
  }
  pagesInitialized = true;
}

void setPageLine(uint8_t pageIndex, uint8_t row, const String& text) {
  if (pageIndex >= PAGE_COUNT) return;
  pages[pageIndex].setLine(row, text);
}

// ---------------------- I2C Helpers ----------------------
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
    initLcdPages();
    Serial.println("LCD connected.");
  }
  
  // RTC (optional) - CHANGE HERE: I2C address if needed.
  if (isI2CDeviceConnected(0x68)) {
    rtcAvailable = rtc.begin();
    Serial.println("RTC connected.");
  }
}

// ---------------------- Wi-Fi Helpers ----------------------
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

// ---------------------- Display ----------------------
void updateDisplay() {
  if (!lcdAvailable) return;
  if (!pagesInitialized) initLcdPages();

  DateTime now = getCurrentDateTime();
  const char* dayNames[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};

  char lineBuffer[LCD_COLS + 1];
  snprintf(lineBuffer, sizeof(lineBuffer), "%s   %02d:%02d",
           dayNames[now.dayOfTheWeek()], now.hour(), now.minute());

  // CHANGE HERE: page content (page index, row index, text).
  // Page 0 uses the previous default content:
  setPageLine(0, 0, lineBuffer);
  setPageLine(0, 1, "IP:" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("--")));
  setPageLine(1, 0, "Mode: " + String(shabbatMode ? "Shabbat " : "Week    "));
  setPageLine(1, 1, "Clock Status:" + String(relay_state ? " ON" : "OFF"));

  // Timed page rotation.
  unsigned long nowMs = millis();
  unsigned long intervalMs = pages[currentPage].intervalMs;
  if (intervalMs > 0 && (nowMs - lastPageSwitch >= intervalMs)) {
    currentPage = (currentPage + 1) % PAGE_COUNT;
    lastPageSwitch = nowMs;
  }

  // Render the active page.
  pages[currentPage].render(lcd);

  // Wi-Fi indicator (custom char 0) at top right corner.
  lcd.setCursor(pages[currentPage].cols - 1, 0);
  if (WiFi.status() == WL_CONNECTED) lcd.write((byte)0);
  else lcd.print("X");
}
