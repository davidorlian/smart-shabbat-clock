#ifndef FIREBASE_CONFIG_H
#define FIREBASE_CONFIG_H

// Copy this file to firebase_config.h and fill local credentials there.
// firebase_config.h is gitignored and must not be committed.

#define FIREBASE_API_KEY "YOUR_WEB_API_KEY"
#define FIREBASE_DATABASE_URL "https://smart-shabbos-clock-default-rtdb.europe-west1.firebasedatabase.app"
#define FIREBASE_DEVICE_EMAIL "device@example.com"
#define FIREBASE_DEVICE_PASSWORD "YOUR_DEVICE_PASSWORD"
#define FIREBASE_DEVICE_ID "shabbat-clock-01"

// First version uses HTTPS REST with WiFiClientSecure.
// Set to 0 only if you add and maintain the correct root CA certificate.
#define FIREBASE_ALLOW_INSECURE_TLS 1

#endif // FIREBASE_CONFIG_H
