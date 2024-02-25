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
       !!cette version est mono instance sur le port HTTP_PORT (80 par defaut)
    

     *************************************************/
#pragma once
#include <Arduino.h>
#define NO_DEBUG
#include "EventsManager32.h"
#define DEBUG_ESP_HTTP_SERVER
#include <ESP8266WebServer.h>
//#include <ESP8266mDNS.h>



extern ESP8266WebServer ServerHttp;


class evHandlerHttp : public eventHandler_t {
public:
  evHandlerHttp(const uint8_t aEventCode);
  virtual void begin() override;
  //virtual byte get() override;
  virtual void handle() override;
  
private:
  uint8_t evCode;
};


