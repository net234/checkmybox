/*************************************************
 *************************************************
    handler evUdp.h   validation of lib betaEvents to deal nicely with events programing with Arduino
    Copyright 2020 Pierre HENRY net23@frdev.com All - right reserved.

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

   works with beteEvents 2.0

    V1.0  07/09/2022
    - gestion de message UDP pour communiquer en node avec des events

     *************************************************/
#if defined(ESP8266) || defined(ESP32)

#include  "evHandlerUdp.h"
//#include <WiFiUdp.h>

//WiFiUDP UDP;

//const uint16_t delaySilenceUdp = 300;  // delay de silence avant d'envoyer les trames
const uint16_t delayInterUdp = 100;     // delay entre 2 trames
const uint8_t  numberOfTrame = 4;      // nombre de trame repetitives


evHandlerUdp::evHandlerUdp(const uint8_t aEventCode, const uint16_t aPortNumber,  String& aNodename) :
  evCode(aEventCode),
  localPortNumber(aPortNumber),
  nodename(aNodename) {
  //rxHeader.reserve(16);
  //rxNode.reserve(16);
  rxJson.reserve(UDP_MAX_SIZE);
  messageUDP.reserve(UDP_MAX_SIZE);
  //  01:10:47.989 -> 01:10:47,CPU=18%,Loop=884,Nill=778,Ram=46456,Frag=15%,MaxMem=38984 Miss:16/0
  //  01:25:14.369 -> 01:25:13,CPU=22%,Loop=849,Nill=745,Ram=46464,Frag=2%,MaxMem=45608 Miss:12/0

}

const  IPAddress broadcastIP(255, 255, 255, 255);

void evHandlerUdp::begin() {
  UDP.begin(localPortNumber);
}

void evHandlerUdp::handle() {
  if (evManager.code  == evCode) {

    // broadcst out = send unicast  castCnt  fois

    switch (evManager.ext) {
      case evxBcast: {
          if (castCnt == 0) return;
          // send udp after 200ms of silence
          if (millis() - lastUDP > delaySilenceUdp) {
            cast(txIPDest);
            --castCnt;
          }
          D_println(castCnt);
          if (castCnt > 0) evManager.delayedPushMilli(delayInterUdp, evCode, evxBcast);
        }
        break;
    }
    return;
  }

  // check for reception
  if (evManager.code != evNill) return;
  int packetSize = UDP.parsePacket();
  if (packetSize == 0) return;

  T_println("Received packet UDP");
  //  Serial.printf("Received packet of size % d from % s: % d\n    (to % s: % d, free heap = % d B)\n",
  //                packetSize,
  //                UDP.remoteIP().toString().c_str(), UDP.remotePort(),
  //                UDP.destinationIP().toString().c_str(), UDP.localPort(),
  //                ESP.getFreeHeap());

  char udpPacketBuffer[UDP_MAX_SIZE + 1]; //buffer to hold incoming packet,
  int size = UDP.read(udpPacketBuffer, UDP_MAX_SIZE);

  //  // read the packet into packetBufffer
  //  if (packetSize > UDP_MAX_SIZE) {
  //    Serial.printf("UDP too big ");
  //    return;
  //  }

  //TODO: clean this   cleanup line feed
  if (size > 0 && udpPacketBuffer[size - 1] == '\n') size--;
  udpPacketBuffer[size] = 0;

  String aStr = udpPacketBuffer;

  lastUDP = millis();

  // filtrage des trame repetitive
  
  String bStr = grabFromStringUntil(aStr, ','); // should be {"TRAME":xxx,
  String cStr = grabFromStringUntil(bStr, ':'); // should be {"TRAME"

  if ( not ( cStr.equals(F("{\"TRAME\"")) and aStr.endsWith("}}}") ))  {  //
    TD_print("Bad paquet",cStr);
    D_println(aStr);
    return;
  }
  byte trNum = bStr.toInt();

  // UdpId is a mix of remote IP and EVENT number
  rxIPSender = UDP.remoteIP();
  IPAddress  aUdpId = rxIPSender;
  aUdpId[0] = trNum;

  //C'est un doublon USP
  if (aUdpId == lastUdpId) {
    T_println("Doublon UDP");
    return;
  }
  //Todo : filtrer les 5 dernier UdpID ?
  lastUdpId = aUdpId;
 
  // c'est une nouvelle trame


  bcast = ( UDP.destinationIP() == broadcastIP );

  // Analyse de la suite : normalement "nodename":{ json struct }"

  

  rxJson = '{';
  rxJson += aStr;
     // D_print(rxHeader);
     // D_print(rxNode);
      D_println(rxJson);

  evManager.push(evCode, evxUdpRxMessage);

}


void evHandlerUdp::broadcast(const String & aJsonStr) {
  T_println("Send broadcast ");
  unicast(broadcastIP, aJsonStr);
}

void evHandlerUdp::unicast(const IPAddress aIPAddress, const String& aJsonStr) {
  TD_println("Send unicast ", aJsonStr);
  messageUDP = aJsonStr;
  castCnt = numberOfTrame;
  if (++numTrameUDP == 0) numTrameUDP++;
  txIPDest = aIPAddress;
  evManager.delayedPushMilli(0, evCode, evxBcast); // clear pending bcast
}

// {"WiFiLuce":"{\"temperature_radiateur\":21.25}","DOMO02":"{\"ext\":21.25}"}
void evHandlerUdp::cast(const IPAddress aAddress) {
  T_println("Send cast ");
  String message = F("{\"TRAME\":");
  message += numTrameUDP;
  message += ",\"";
  message += nodename;
  message += "\":";
  message += messageUDP;
  message += '}';
  if ( !UDP.beginPacket(aAddress, localPortNumber) ) {
    D_println("Error sending UDP");
    return ;
  }
  TD_println("UDP send", message);
  UDP.write(message.c_str(), message.length());
  UDP.endPacket();
}

#endif
