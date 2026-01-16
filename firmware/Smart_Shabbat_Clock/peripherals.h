#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <stdint.h>

class LiquidCrystal_I2C;
class RTC_DS3231;

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
