# Smart Shabbat Clock

ESP32-based smart Shabbat clock for scheduled control of household loads, featuring RTC/NTP timekeeping, a web-based user interface, and RF communication to remote Shabbat switch units (HC-12).

## Demo
- QR/Redirect: https://davidorlian.github.io/smart-shabbat-clock/demo/
- Video: https://youtube.com/shorts/kuyBU1jBy3k

## Project Book
- Hebrew (PDF): `docs/project-book/Smart_Shabbat_Clock_Project_Book_HE.pdf`

## Hardware (Main Unit)
- ESP32 (Wi-Fi)
- RTC: DS3231
- LCD 1602 (I2C)
- RF module: HC-12
- (Optional) Relay/SSR driver stage (depends on build)

## Repository Structure
- `firmware/` – ESP32 firmware (Arduino)
- `docs/` – Documentation and demo redirect page (`docs/demo/`)

## Key Features
- Weekly scheduling (ON/OFF events)
- Automatic Shabbat mode support
- NTP time sync with RTC (DS3231) fallback
- Web UI for configuration and monitoring
- RF communication to remote switch units (HC-12)

## Quick Start (Arduino IDE)
1. Open: `firmware/Smart_Shabbat_Clock/Smart_Shabbat_Clock.ino`
2. Edit Wi-Fi credentials in `Smart_Shabbat_Clock.ino`:
   - `const char* ssid     = "YOUR_SSID_HERE";`
   - `const char* password = "YOUR_PASSWORD_HERE";`
3. Select your ESP32 board + COM port in Arduino IDE.
4. Install required libraries (below).
5. Upload.
6. Open Serial Monitor to see the device IP, then browse to `http://<ip>/`.

## Dependencies
The firmware uses common Arduino/ESP32 libraries, including:
- `WiFi.h`
- `WebServer.h` (ESP32 WebServer)
- `Preferences.h`
- `RTClib.h` (DS3231)
- `LiquidCrystal_I2C.h`
- `time.h`

## Safety Note
This project can control real household loads. Use proper isolation, fusing, and certified mains-rated hardware.

## Authors
- Barak Ashwal
- David Orlian
