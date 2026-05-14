class WebServer;

#ifndef WEB_API_H
#define WEB_API_H

#include <WebServer.h>

// Web server setup/handlers.
void initWebServer();
extern WebServer server;
void saveRelayMode(uint8_t mode);
void saveShabbatMode(bool mode);

#endif // WEB_API_H
