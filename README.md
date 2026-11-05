# Smart Shabbat Clock

ESP32-based smart Shabbat clock for scheduled control of household electrical loads.

The system combines local scheduling, RTC/NTP timekeeping, a local web interface, relay control, and RF communication to remote Smart Shabbat Switch units using HC-12. Optional Firebase cloud control is also supported for remote monitoring and command sending.

## Demo

* QR/Redirect: https://davidorlian.github.io/smart-shabbat-clock/demo/
* Video: https://youtube.com/shorts/kuyBU1jBy3k

## Project Book

* Hebrew project book: `docs/project-book/Smart_Shabbat_Clock_Project_Book_HE.pdf`

## System Overview

The main unit is based on an ESP32 and acts as the central controller of the system.

Its main responsibilities are:

* Managing a weekly ON/OFF schedule
* Controlling the local relay output
* Keeping accurate time using NTP and DS3231 RTC fallback
* Providing a local web interface on the home network
* Communicating with remote Smart Shabbat Switch units using HC-12
* Optionally syncing status and commands through Firebase

The local ESP32 logic remains the core of the system. Firebase is used only as an optional remote control and status layer.

## Hardware

Main unit:

* ESP32
* DS3231 RTC module
* LCD 1602 with I2C interface
* HC-12 RF module
* Relay or SSR driver stage, depending on the build
* AC-DC power supply module

Remote units:

* Smart Shabbat Switch units controlled by RF commands from the main unit

## Key Features

* Weekly schedule for relay ON/OFF events
* Automatic and manual relay control
* Shabbat / weekday mode support
* NTP time synchronization
* RTC fallback after power loss or network loss
* Local web interface served directly by the ESP32
* HC-12 communication with remote switch units
* Optional Firebase cloud control panel
* Firebase Realtime Database security rules for admin/device separation

## Repository Structure

```text
firmware/
  Smart_Shabbat_Clock/
    ESP32 firmware for the main unit

web/
  Firebase Hosting cloud control page

tools/
  Local validation and maintenance scripts

docs/
  Project documentation, demo redirect, and project book

database.rules.json
  Firebase Realtime Database security rules

firebase.json
  Firebase Hosting configuration
```

## Local Configuration Files

The project uses local configuration files for credentials and private settings.

These files must not be committed:

```text
.env.local
web/firebase-config.js
firmware/Smart_Shabbat_Clock/wifi_config.h
firmware/Smart_Shabbat_Clock/firebase_config.h
```

Use the matching example files as templates:

```text
.env.local.example
web/firebase-config.example.js
firmware/Smart_Shabbat_Clock/wifi_config.example.h
firmware/Smart_Shabbat_Clock/firebase_config.example.h
```

## Firmware Setup

1. Open the firmware in Arduino IDE:

```text
firmware/Smart_Shabbat_Clock/Smart_Shabbat_Clock.ino
```

2. Create local Wi-Fi configuration:

```text
Copy:
firmware/Smart_Shabbat_Clock/wifi_config.example.h

To:
firmware/Smart_Shabbat_Clock/wifi_config.h
```

Then edit:

```cpp
#define WIFI_SSID "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"
```

3. Optional: create Firebase device configuration for cloud sync:

```text
Copy:
firmware/Smart_Shabbat_Clock/firebase_config.example.h

To:
firmware/Smart_Shabbat_Clock/firebase_config.h
```

Then edit:

```cpp
#define FIREBASE_API_KEY "your-firebase-web-api-key"
#define FIREBASE_DEVICE_EMAIL "device-user-email"
#define FIREBASE_DEVICE_PASSWORD "device-user-password"
#define FIREBASE_DEVICE_ID "shabbat-clock-01"
```

4. Select the correct ESP32 board and COM port.

5. Install the required Arduino libraries.

6. Upload the firmware.

7. Open Serial Monitor and use the printed IP address to access the local web interface:

```text
http://<device-ip>/
```

If mDNS is available on the local network, the device may also be reachable at:

```text
http://shabbat-clock.local
```

## Firebase Cloud Control

Firebase support is optional. The clock can still operate locally without Firebase.

The cloud path is:

```text
Firebase Hosting web page
        ↓
Firebase Realtime Database
        ↓
ESP32 cloud sync
        ↓
Local firmware logic
```

Firebase is used for remote command sending and status viewing. The ESP32 still performs the actual control logic locally.

Firebase services used:

* Firebase Authentication
* Firebase Realtime Database
* Firebase Hosting

The system uses two Firebase users:

* **Admin user**: used by the web app to read state and create commands
* **Device user**: used by the ESP32 to read its commands and write its own status/results

## Firebase Web Setup

Create a local Firebase web config:

```powershell
Copy-Item web\firebase-config.example.js web\firebase-config.js
```

Edit `web/firebase-config.js` with the Firebase Web App config from the Firebase Console.

Run the web page locally:

```powershell
cd web
python -m http.server 5000
```

Open:

```text
http://localhost:5000
```

Deploy to Firebase Hosting:

```powershell
npx firebase-tools deploy --only hosting --project smart-shabbos-clock
```

## Firebase Rules

Realtime Database rules are stored in:

```text
database.rules.json
```

Deploy rules:

```powershell
npx firebase-tools deploy --only database --project smart-shabbos-clock
```

Validate rules against the real Firebase project:

```powershell
Copy-Item .env.local.example .env.local
node tools\validate-firebase-rules.mjs
```

The `.env.local` file is local only and must not be committed.

## Local Control vs Cloud Control

The local ESP32 web interface sends commands directly to the ESP32 on the local network and should respond quickly.

The Firebase cloud interface sends commands through Realtime Database. The ESP32 polls Firebase, applies the command locally, and writes a result back to Firebase. This path may be slower than the local interface.

Cloud command results are separate from HC-12 ACK messages used for communication with remote switch units.

## Dependencies

Firmware dependencies include common Arduino / ESP32 libraries:

* `WiFi.h`
* `WebServer.h`
* `HTTPClient.h`
* `Preferences.h`
* `Wire.h`
* `RTClib.h`
* `LiquidCrystal_I2C.h`
* `time.h`

Development / validation tools:

* Node.js
* Firebase CLI

## Safety Note

This project can control real household electrical loads.

Use proper isolation, fusing, and mains-rated hardware. Do not expose unsafe mains wiring. Do not use relay or SSR modules beyond their rated voltage/current. Testing should be done carefully and with suitable electrical protection.

## Authors

* Barak Ashwal
* David Orlian
