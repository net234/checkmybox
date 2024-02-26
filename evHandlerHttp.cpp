/*************************************************
 *************************************************
    handlerHttp.h   validation of lib betaEvents to deal nicely with events programing with Arduino
    Copyright 2024 Pierre HENRY net23@frdev.com All - right reserved.

  This file is part of betaEvents.

    betaEvents is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    betaEvents is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with betaEvents.  If not, see <https://www.gnu.org/licenses/lglp.txt>.

  History

   works with betaEvents32 3.0

    V1.0  24/02/2024
    - gestion d'appel http pour communiquer avec une appli en http avec des reponces en json

    

     *************************************************/

#include "evHandlerHttp.h"
//#include <ESP8266mDNS.h>





#ifndef HTTP_PORT
#define HTTP_PORT 80
#endif

ESP8266WebServer ServerHttp(HTTP_PORT);

void handleRoot() {
  //digitalWrite(led, 0);
  ServerHttp.send(200, "text/plain", "hello from esp8266!");
  //digitalWrite(led, 1);
}

void handleNotFound() {
  //digitalWrite(led, 0);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += ServerHttp.uri();
  message += "\nMethod: ";
  message += (ServerHttp.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += ServerHttp.args();
  message += "\n";
  for (uint8_t i = 0; i < ServerHttp.args(); i++) {
    message += " " + ServerHttp.argName(i) + ": " + ServerHttp.arg(i) + "\n";
  }
  ServerHttp.send(404, "text/plain", message);
  //digitalWrite(led, 1);
}

void handleApi() {
  //{"status":true,"message":"Ok","answer":{"result":true,"delta":1.95}}
  {
    JSONVar aJson, bJson;
    if (buildApiAnswer(aJson, ServerHttp.argName(0), ServerHttp.arg(0))) {
      bJson["status"] = true;
      bJson["message"] = "Ok";
      bJson["answer"] = aJson;
    } else {
      bJson["status"] = false;
      bJson["message"] = "Error";
    }

    ServerHttp.send(200, "application/json", JSON.stringify(bJson));
    DV_println(helperFreeRam());
  }
}
evHandlerHttp::evHandlerHttp(const uint8_t aEventCode)
  : evCode(aEventCode){};


void evHandlerHttp::begin() {
  ServerHttp.on("/", handleRoot);

  ServerHttp.on("/api.json", handleApi);

  ServerHttp.onNotFound(handleNotFound);

  ServerHttp.begin();
  DT_println("HTTP server started");
  ServerHttp.begin();
}

void evHandlerHttp::handle() {
  ServerHttp.handleClient();
  //MDNS.update();
}
