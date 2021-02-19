#include <ESP8266WiFi.h>  // ---------- core 2.3.0 --------------------
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include "SuplaHTTPUpdateServer.h"

extern "C" {
#include "user_interface.h"
}

#ifdef USE_SPIFFS
#include "FS.h"
#define CONFIG_FILE_PATH "/dat"

#define MAX_GUID     16
#define MAX_AUTHKEY  16
#define MAX_SSID     32
#define MAX_PASSWORD 64

char ssid[MAX_SSID];
char password[MAX_PASSWORD];
#endif

ESP8266HTTPUpdateServer httpUpdater;
ESP8266WebServer httpServer(80);
int timeoutCounter;

void setup() {
  Serial.begin(74880);
#ifdef USE_SPIFFS
  if (SPIFFS.begin()) {
    File configFile = SPIFFS.open(CONFIG_FILE_PATH, "r");

    if (configFile) {
      uint8_t length = MAX_GUID + MAX_AUTHKEY + MAX_SSID + MAX_PASSWORD;
      uint8_t *html = (uint8_t *)malloc(sizeof(uint8_t) * length);

      configFile.read(html, length);
      configFile.close();

      strncpy(ssid, (const char *)(html + 34), MAX_SSID);
      strncpy(password, (const char *)(html + 67), MAX_PASSWORD);

      if (strcmp(ssid, "") != 0) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        timeoutCounter = 10;

        while (WiFi.status() != WL_CONNECTED && timeoutCounter > 0) {
          delay(1000);
          timeoutCounter--;
        }
      }
    }
  }
#endif
  if (WiFi.status() != WL_CONNECTED) {
    timeoutCounter = 10;
    IPAddress apIP(192, 168, 4, 1);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    while (!WiFi.softAP("Updater SUPLA", NULL) && timeoutCounter > 0) {
      delay(1000);
      timeoutCounter--;
    }
  }

  httpUpdater.setup(&httpServer, "/");
  httpServer.onNotFound(handleNotFound); 
  httpServer.begin();
}

void handleNotFound()
{ 
    httpServer.sendHeader("Location", "/",true); //Redirect to our html web page 
    httpServer.send(302, "text/plane",""); 
}

void loop() {
  httpServer.handleClient();
}
