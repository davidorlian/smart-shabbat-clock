#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <Arduino.h>

class LiquidCrystal_I2C;
class RTC_DS3231;

// ---------------------- LCD Pages ----------------------
// CHANGE HERE: LCD size used by the page system and LCD object.
static const uint8_t LCD_COLS = 16;
static const uint8_t LCD_ROWS = 2;
// CHANGE HERE: add page-related helpers if you expand the system.
static const uint8_t MAX_LCD_ROWS = 4;
static const uint8_t MAX_LCD_COLS = 20;

struct LcdPage {
  uint8_t cols = 0;
  uint8_t rows = 0;
  unsigned long intervalMs = 0;
  String lines[MAX_LCD_ROWS];

  // Set up size + rotation interval (ms).
  void init(uint8_t colsIn, uint8_t rowsIn, unsigned long intervalMsIn);
  // Store text for one row (row starts at 0). Text is clipped/padded to fit.
  void setLine(uint8_t row, const String& text);
  // Clear all rows to spaces.
  void clear();
  // Print stored rows to the LCD.
  void render(LiquidCrystal_I2C& lcd) const;
};

// Create pages and set default sizes/intervals.
void initLcdPages();
// Write a line into a specific page (page index starts at 0).
void setPageLine(uint8_t pageIndex, uint8_t row, const String& text);

// Optional hardware (LCD/RTC) and Wi-Fi helpers.
// CHANGE HERE: add new helper declarations if you add features.
bool isI2CDeviceConnected(uint8_t address);
void initPeripheralsSafely();
void updateDisplay();
void connectToWiFi();
void handleWiFiReconnect();

extern LiquidCrystal_I2C lcd;
extern RTC_DS3231 rtc;
extern bool lcdAvailable;
extern bool rtcAvailable;
extern unsigned long lastWiFiAttempt;
extern const uint8_t SDA_PIN;
extern const uint8_t SCL_PIN;

#endif // PERIPHERALS_H
