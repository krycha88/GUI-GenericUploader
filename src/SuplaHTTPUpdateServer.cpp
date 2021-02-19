#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include "SuplaHTTPUpdateServer.h"

const char* ESP8266HTTPUpdateServer::_serverIndex =
    "<form method='POST' action='' enctype='multipart/form-data'><input type='file' accept='.bin,.bin.gz' name='firmware'><input type='submit' "
    "value='Update'></form>";
const char* ESP8266HTTPUpdateServer::_failedResponse = "Update Failed!";
const char* ESP8266HTTPUpdateServer::_successResponse = "<META http-equiv=\"refresh\" content=\"15;URL=\">Update Success! Rebooting...";
const char* ESP8266HTTPUpdateServer::_rebootResponse = "<META http-equiv=\"refresh\" content=\"15;URL=\">Rebooting...";

ESP8266HTTPUpdateServer::ESP8266HTTPUpdateServer(bool serial_debug) {
  _serial_output = serial_debug;
  _server = NULL;
  _username = NULL;
  _password = NULL;
  _authenticated = false;
}

void ESP8266HTTPUpdateServer::setup(ESP8266WebServer* server, const char* path, const char* username, const char* password) {
  _server = server;
  _username = (char*)username;
  _password = (char*)password;

  // handler for the /update form page
  _server->on(path, HTTP_GET, [&]() {
    if (_username != NULL && _password != NULL && !_server->authenticate(_username, _password))
      return _server->requestAuthentication();
    String sCommand = _server->arg("cmd");
    if (strcasecmp_P(sCommand.c_str(), PSTR("reboot")) == 0) {
      _server->sendHeader("Location", "/", true);
      _server->send(200, "text/html", _rebootResponse);
      ESP.restart();
    }
    else {
      _server->send(200, "text/html", handleIndex());
    }
  });

  // handler for the /update form POST (once file upload finishes)
  _server->on(
      path, HTTP_POST,
      [&]() {
        if (!_authenticated)
          return _server->requestAuthentication();
        _server->send(200, "text/html", Update.hasError() ? _failedResponse : _successResponse);
        ESP.restart();
      },
      [&]() {
        // handler for the file upload, get's the sketch bytes, and writes
        // them through the Update object
        HTTPUpload& upload = _server->upload();
        if (upload.status == UPLOAD_FILE_START) {
          if (_serial_output)
            Serial.setDebugOutput(true);

          _authenticated = (_username == NULL || _password == NULL || _server->authenticate(_username, _password));
          if (!_authenticated) {
            if (_serial_output)
              Serial.printf("Unauthenticated Update\n");
            return;
          }

          WiFiUDP::stopAll();
          if (_serial_output)
            Serial.printf("Update: %s\n", upload.filename.c_str());
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if (!Update.begin(maxSketchSpace)) {  // start with max available size
            if (_serial_output)
              Update.printError(Serial);
          }
        }
        else if (_authenticated && upload.status == UPLOAD_FILE_WRITE) {
          if (_serial_output)
            Serial.printf(".");
          if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            if (_serial_output)
              Update.printError(Serial);
          }
        }
        else if (_authenticated && upload.status == UPLOAD_FILE_END) {
          if (Update.end(true)) {  // true to set the size to the current progress
            if (_serial_output)
              Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
          }
          else {
            if (_serial_output)
              Update.printError(Serial);
          }
          if (_serial_output)
            Serial.setDebugOutput(false);
        }
        else if (_authenticated && upload.status == UPLOAD_FILE_ABORTED) {
          Update.end();
          if (_serial_output)
            Serial.println("Update was aborted");
        }
        delay(0);
      });
}

String ESP8266HTTPUpdateServer::handleIndex() {
  String html = "";

  html += "<b>GUI-Generic Updater</b><br><br>";
  html += "<div>Flash Size: ";
  html += ESP.getFlashChipRealSize() / 1024;
  html += " kB</div>";
  html += "<div>Sketch Max Size: ";
  html += ESP.getFreeSketchSpace() / 1024;
  html += " kB</div>";
  html += _serverIndex;
  html += "<form>";
  html += "<a class='button-link' href='/?cmd=reboot'>Reboot</a>";
  html += "</form>";

  return html;
}
