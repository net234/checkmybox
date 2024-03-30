/*************************************************
 *************************************************
    Sketch checkmybox.ino.ino   check and report  a box and the wifi connectivity
    Copyright 2021 Pierre HENRY net23@frdev.com All - right reserved.

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
    V1.0 (21/10/2021)
    - From betaevent.h + part of betaport V1.2
      to check my box after a problem with sfr :)
    V1.1 (25/10/2021)
   TODO: beter use of get/set config (add some cache ?)
   NOTODO: replace webClock with ntptime ?
   DONE: add a 1wire temp sensor to send to API ?
    V1.2  (27/10/2021)
    adjust for Betaevent 2.2
    DONE: make a default nodename buid upon mac adresse
    FIXED: bug   lost node name on first init
    FIXED: bug   MAILTO not updated in global when changed
    final version with arduino 32bit time_t
    V1.3  (19/10/2023)
    simplification de evHandlerDS18x20 pour gerer de multiple sondes  TODO: utiliser les EXT2 de la version 32
    ajout de l'OTA actif 5 minutes apres le boot
    checkMyBox V1.3.B2
    ajout Bandeau de led
    deplacement des XXXX_PIN dans ESP8266.h
    unjout d'un etat postInit pour activer les notification slack uniquement 30 secondes apres le boot

    checkMyBox V1.4  (27/10/2024)
    Ajout API               TODO: a migrer dans betaevent32
    Ajout LED  SK9822       TODO: faire un handler pour betaEvent32


 *************************************************/

/*
  //25/02/2024  V1.4a ajout  webserveur pour faire une api  via un handlerHttp
  //27/02/2024  V1.4b ajout  bp1 toogle pour gerer l'allumage hall  // http://sonoff1.local/api.json?action=relayToggle  http://10.11.12.57/
  //17:26:39.040 -> {"nodename":"bNode02","timezone":-1,"smtpserver":"smtp.beta","mailfrom":"NODE@betamachine.fr","mailto":"net234@frdev.com","sondename":"cuisine,hall"}
  // PC-PHY  {"CMD":{"bNode02":"10.11.12.57/api.json?action=relay_toggle"}}
  . Variables and constants in RAM (global, static), used 32156 / 80192 bytes (40%)
  . Instruction RAM (IRAM_ATTR, ICACHE_RAM_ATTR), used 62223 / 65536 bytes (94%)
  . Code in flash (default, ICACHE_FLASH_ATTR), used 488468 / 1048576 bytes (46%)

  V1.5A  20/03/2024 reprise basée sur bNodeet betaEvent32 en librairie
. Variables and constants in RAM (global, static), used 32160 / 80192 bytes (40%)
. Instruction RAM (IRAM_ATTR, ICACHE_RAM_ATTR), used 62223 / 65536 bytes (94%)
. Code in flash (default, ICACHE_FLASH_ATTR), used 487684 / 1048576 bytes (46%)
  
*/

#define APP_NAME "checkmybox V1.5 C"
static_assert(sizeof(time_t) == 8, "This version works with time_t 32bit  moveto ESP8266 kernel 3.0");


/* Evenements du Manager (voir EventsManager.h)
  evNill = 0,      // No event  about 1 every milisecond but do not use them for delay Use delayedPushMilli(delay,event)
  ev100Hz,         // tick 100HZ    non cumulative (see betaEvent.h)
  ev10Hz,          // tick 10HZ     non cumulative (see betaEvent.h)
  ev1Hz,           // un tick 1HZ   cumulative (see betaEvent.h)
  ev24H,           // 24H when timestamp pass over 24H
  evInit,
  evInChar,
  evInString,
*/



// Liste des evenements specifique a ce projet
enum tUserEventCode {
  // evenement utilisateurs
  evBP0 = 100,
  evBP1,
  evLed0,
  evRefreshLed,
  evStartAnim,        //Allumage Avec l'animation
  evNextStep,         //etape suivante dans l'animation
  evTimeMasterGrab,   //Annonce le placement en MasterTime
  evTimeMasterSyncr,  //Signale aux bNodes periodiquement la presence du masterTime
  evCheckWWW,
  evCheckAPI,
  evUdp,
  evHttp,
  // evenement action
  doReset,
};
const uint32_t checkWWW_DELAY = (60 * 60 * 1000L);
const uint32_t checkAPI_DELAY = (2 * 60 * 1000L);
const uint32_t DS18X_DELAY = (2 * 60 * 1000L);  // lecture des sondes toute les 5 minutes

// instance betaEvent
// BetaEvents32.h  installe :
//  - une instance "MyEvents" pour gere les evenements
// poussoir "BP0"
// une LED "Led0"
// un clavier "Keyboard"
// un debugger "Debug"
//  MyBP0 genere un evenement evBP0 a chaque pression le poussoir connecté sur D2
//  Led0 genere un evenement evLed0 a chaque clignotement de la led precablée sur la platine
//  Keyboard genere un evenement evChar a char caractere recu et un evenement evString a chaque ligne recue
//  MyDebug permet sur reception d'un "T" sur l'entrée Serial d'afficher les infos de charge du CPU
//#define DEBUG_ON
#include "ESP8266.H"       // assignation des pin a l'aide constant #define type XXXXXX_PIN
#include <BetaEvents32.h>  // Instance Events
#include <ESP8266mDNS.h>
#include <bNodesTools.h>    // Instence bNode base bnodes pour la gestion des parametres, du journal (littleFs) et de l'heure (currentTime)
#include <evHandlerWifi.h>  // Create Wifi instance named Wifi
#include <Arduino_JSON.h>
evHandlerWifi Wifi;  // instance of WifiHandler

// BP0 est créé automatiquement par BetaEvent.h
// dans la version actuelle BP1
// avec HALLKEY  envois message   TOGGLE a la device http://HALLKEY    sonOff01.local/relay_toggle
// avec SKEY     envois a cleak avec la SKEY   le lab est ouver ou le lab est fermé
// TODO: a passer en scriptEvent
evHandlerButton BP1(evBP1, BP1_PIN, 5000);  // pousssoir externe  5 secondes pour le long down/up



// Sondes temperatures : DS18B20
//instance du bus OneWire dedié aux DS18B20
#include "evHandlerDS18b20.h"

evHandlerDS18b20 mesDS18(DS18X_DELAY, ONEWIRE_PIN);


// leds WS2812   3 leds fixes 17 ledes en animation (chenillard)
#define ledsMAX 10
//#include "WS2812.h"
#include "SK9822.h"
#define ledRVB_t SK9822rvb_t
ledRVB_t ledFixe1;
ledRVB_t ledFixe2;
ledRVB_t ledFixe3;
// Array contenant les leds d'animation
ledRVB_t leds[ledsMAX];

// Variable d'application locale



//
//time_t currentTime;   // unix timestamp local time
//int8_t timeZone = 0;  //-2;  //les heures sont toutes en localtimes (par defaut hivers france)


time_t webClockLastTry = 0;
uint32_t bootedSecondes = 0;
int webClockDelta = 0;
int currentMonth = -1;

//bool WiFiConnected = false;
const int delayTimeMaster = 60 * 1000;  // par defaut toute les Minutes
bool isTimeMaster = false;
//bool configErr = false;
bool WWWOk = false;
bool APIOk = false;
bool sleepOk = true;
int multi = 0;  // nombre de clic rapide
//bool configOk = true;  // global used by getConfig...

bool postInit = false;  // true postInitDelay secondes apres le boot (limitation des messages Slack)
String mailSendTo;      // mail to send email

int8_t sondesNumber = 0;  // nombre de sonde
#define MAXDS18b20 10
String sondesName[MAXDS18b20];  // noms des sondes
//float sondesValue[MAXDS18b20];  // valeur des sondes
const int8_t switchesNumber = 2;
String switchesName[switchesNumber];
int8_t displayStep = 0;
// init UDP
#include "evHandlerUdp.h"
const unsigned int localUdpPort = 23423;  // local port to listen on
evHandlerUdp myUdp(evUdp, localUdpPort, Wifi.nodeName);
e_rvb ledLifeColor = rvb_white;
String skey;
String hallkey;
bool shortBP1 = false;
JSONVar myDevices, meshDevices;

#include "evHandlerHttp.h"
evHandlerHttp myHttp(evHttp);




void setup() {
  //Serial.begin(115200);
  //Serial.println(F("\r\n\n" APP_NAME));
  //DV_println(sizeof(stdEvent_t));
  //delay(3000);
  // Start instance
  Events.begin();
  Serial.println(F("\r\n\n" APP_NAME));
  //DV_println(WiFi.getMode());
  /*
  // System de fichier
  // il gere
  if (!MyLittleFS.begin()) {
    Serial.println(F("erreur MyLittleFS"));
    fatalError(3);
  }
*/
  /*
    // recuperation de l'heure dans la static ram de l'ESP
    coldBoot = !getRTCMemory();  // si RTC memory est vide c'est un coldboot
    if (coldBoot) {
    // si l'heure n'est pas presente
    // c'est un cold boot
    // on recupere l'heure du fichier Histo par defaut
    savedRTCmemory.actualTimestamp = jobGetHistoTime() + 10;
    DV_println(savedRTCmemory.actualTimestamp);
    }


    // little trick to leave timeStatus to timeNotSet
    // TODO: see with https://github.com/PaulStoffregen/Time to find a way to say timeNeedsSync

    currentTime = savedRTCmemory.actualTimestamp + 1;
    adjustTime(currentTime);
  */

  bool configErr = false;
  // recuperation de la config dans config.json
  String nodeName = jobGetConfigStr(F("nodename"));
  if (nodeName == "") {
    Serial.println(F("!!! Configurer le nom de la device avec 'NODE=nodename' !!!"));
    configErr = true;
    nodeName = "bNode_";
    nodeName += WiFi.macAddress().substring(12, 14);
    nodeName += WiFi.macAddress().substring(15, 17);
  }
  Wifi.nodeName = nodeName;
  DV_println(nodeName);

  // recuperation de la timezone dans la config
  bNode.timeZone = jobGetConfigInt(F("timezone"));
  if (!bNode.configOk) {
    bNode.timeZone = -2;  // par defaut France hivers
    jobSetConfigInt(F("timezone"), bNode.timeZone);
    Serial.println(F("!!! timezone !!!"));
  }
  DV_println(bNode.timeZone);

  //writeHisto("Info", (coldBoot) ? "ColdBoot" : "Boot");
  //DV_println(niceDisplayTime(currentTime, true));

  // recuperation des donnée pour les mails dans la config
  String aSmtpServer = jobGetConfigStr(F("smtpserver"));
  if (aSmtpServer == "") {
    Serial.println(F("!!! Configurer le serveur smtp 'SMTPSERV=smtp.monserveur.mail' !!!"));
    configErr = true;
  }
  DV_println(aSmtpServer);

  mailSendTo = jobGetConfigStr(F("mailto"));
  if (mailSendTo == "") {
    Serial.println(F("!!! Configurer l'adresse pour le mail  'MAILTO=monAdresseMail' !!!"));
    configErr = true;
  }
  DV_println(mailSendTo);


  String currentMessage;
  if (configErr) {
    currentMessage = F("Config Error");
    beep(880, 500);
    delay(500);
  } else {
    currentMessage = F("device=");
    currentMessage += nodeName;
    // a beep
    beep(880, 500);
    delay(500);
    beep(988, 500);
    delay(500);
    beep(1047, 500);
  }
  DV_println(currentMessage);
  DV_println(helperFreeRam());






  // Recuperation du nom des sondes
  sondesNumber = mesDS18.getNumberOfDevices();
  if (!sondesNumber) {
    DV_println(sondesNumber);
    delay(500);
    sondesNumber = mesDS18.getNumberOfDevices();
  }
  if (sondesNumber > MAXDS18b20) sondesNumber = MAXDS18b20;
  DV_println(sondesNumber);
  jobGetSondeName();

  // recuperation de skey
  skey = jobGetConfigStr(F("skey"));
  DV_println(skey.length());

  // recuperation de hallkey
  hallkey = jobGetConfigStr(F("hallkey"));
  DV_println(hallkey.length());


  //  toute les led a blanc a l'init
  //  pinMode(WS2812_PIN, OUTPUT);
  Serial.println("init led ....");
  digitalWrite(ClkSK9822_PIN, LOW);
  digitalWrite(DataSK9822_PIN, LOW);

#if BP0_PIN == BEEP_PIN
  pinMode(BP0_PIN, INPUT_PULLUP);  //Fix in case BP0_PIN   == BEEP_PIN
#endif

  pinMode(ClkSK9822_PIN, OUTPUT);
  pinMode(DataSK9822_PIN, OUTPUT);
  ledFixe1.setcolor(rvb_red, 80, 5000, 5000);
  ledFixe2.setcolor(rvb_orange, 80, 5000, 5000);
  ledFixe3.setcolor(rvb_green, 80, 5000, 5000);
  for (uint8_t N = 0; N < ledsMAX; N++) {
    leds[N].setcolor(rvb_white, 80, 2000, 2000);
  }



  // Recuperation du nom des switches
  jobGetSwitcheName();

  /*
    Serial.println("Wait a for wifi");
    for (int N = 0; N < 50; N++) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(100);
    }
  */

  jobUpdateLed0();

  Serial.println("Bonjour ....");
  Serial.println("Tapez '?' pour avoir la liste des commandes");
  //DV_println(LED_BUILTIN);
  //DV_println(sizeof(double));
  //DV_println(sizeof(float));
}

byte BP0Multi = 0;


//String niceDisplayTime(const time_t time, bool full = false);
bool buildApiAnswer(JSONVar& answer, const String& action, const String& value) {
  DTV_print("api call", action);
  DV_println(value);
  if (!action.length()) {
    answer["node"]["app"] = APP_NAME;
    answer["node"]["name"] = Wifi.nodeName;
    answer["node"]["date"] = niceDisplayTime(bNode.currentTime, true);
    answer["node"]["booted"] = niceDisplayDelay(bootedSecondes);
    answer["devices"] = meshDevices;
    answer["devices"][Wifi.nodeName] = myDevices;
    ServerHttp.sendHeader("refresh", "60", true);
    return true;
  }
  if (action.equals("CMD") and value.length()) {
    Keyboard.setInputString(value);
    answer["cmd"] = value;
    DTV_println("API CMD", value);
    ServerHttp.sendHeader("refresh", "5;url=api.json", true);
    return true;
  }


  //liste les capteurs du type 'action'='name'    api.Json?temperature=interieur listera le PREMIER capteurs temperature nomé interieur
  if (value.length()) {

    if (myDevices.hasOwnProperty(action) and myDevices[action].hasOwnProperty(value)) {
      DV_println(myDevices[action][value]);
      JSONVar aJson = myDevices[action][value];
      answer[action] = aJson;
      return (true);
    }
    // recherche dans le mesh
    JSONVar keys = meshDevices.keys();
    for (int i = 0; i < keys.length(); i++) {
      String aKey = keys[i];
      if (meshDevices[aKey].hasOwnProperty(action) and meshDevices[aKey][action].hasOwnProperty(value)) {
        JSONVar aJson = meshDevices[aKey][action][value];
        answer[action] = aJson;
        return (true);
      }
    }
    answer[action] = null;
    return true;
  }


  //liste les capteurs du type 'action'    api.Json?temperature listera les capteurs temperature
  bool matched = false;
  if (myDevices.hasOwnProperty(action)) {
    DV_println(myDevices[action]);
    JSONVar aJson = myDevices[action];
    answer[Wifi.nodeName][action] = aJson;
    matched = true;
  }
  // recherche dans le mesh
  JSONVar keys = meshDevices.keys();
  for (int i = 0; i < keys.length(); i++) {
    String aKey = keys[i];
    DV_println(aKey);
    if (meshDevices[aKey].hasOwnProperty(action)) {
      JSONVar aJson = meshDevices[aKey][action];
      answer[aKey][action] = aJson;
      matched = true;
    }
  }
  if (matched) {
    ServerHttp.sendHeader("refresh", "60", true);
    return true;
  }


  return false;
}

void loop() {
  Events.get(sleepOk);
  Events.handle();
  switch (Events.code) {
    case evInit:
      {
        Serial.println("Init");
        String aStr = Wifi.nodeName;
        //aStr += ' ';
        aStr += F(" " APP_NAME);

        writeHisto((Wifi.coldBoot) ? F("ColdBoot") : F("Boot"), aStr);
        Events.delayedPushMilli(3000, evStartAnim);
        Events.delayedPushMilli(2500, evRefreshLed);



        jobUpdateLed0();
      }
      break;

    case evPostInit:
      postInit = true;
      jobUpdateLed0();
      myUdp.broadcastInfo("Boot");
      T_println("PostInit done");


      break;

    case evOta:
      switch (Events.intExt) {
        case evxOff:

          myUdp.broadcastInfo("Stop OTA");
          //writeHisto(F("Stop OTA"), Wifi.nodeName);
          // but restart MDNS
          break;

        case evxOn:
          {
            // start OTA
            myUdp.broadcastInfo("start OTA");
            //end start OTA
          }
          break;
      }
      break;
    // recopie LED0 sur LedVie[0]
    case evLed0:
      switch (Events.ext) {
        case evxBlink:
          ledFixe1.setcolor(ledLifeColor, 50, 50, 100);
          break;
      }
      break;

    case evRefreshLed:
      {
        Events.delayedPushMilli(40, evRefreshLed);
        static unsigned long lastrefresh = millis();
        int delta = millis() - lastrefresh;
        lastrefresh += delta;
        jobRefreshLeds(delta);
      }

      break;


    // mise en route des animations
    case evStartAnim:


      Events.delayedPushMilli(3000, evStartAnim);
      displayStep = 0;
      Events.push(evNextStep);
      //T_println("Started Anim");
      break;

    case evNextStep:
      if (displayStep >= ledsMAX) break;
      //leds[displayStep].setcolor(rvb_lightblue, 40+(displayStep*3), 500,1500);
      leds[displayStep].setcolor(rvb_lightblue, 40, 500, 1500);

      displayStep++;
      if (displayStep < ledsMAX) Events.delayedPushMilli(100, evNextStep);

      break;

    case ev24H:
      {
        String newDateTime = niceDisplayTime(bNode.currentTime, true);
        DV_println(newDateTime);
        writeHisto(F("newDateTime"), String(webClockDelta));
        //webClockDelta = 0;
      }
      break;

    case evTagHisto:
      DT_println("evTagHisto");
      jobSetHistoTime();
      Events.delayedPushMilli(3600L * 1000, evTagHisto);
      break;

    case evWifi:
      {
        jobUpdateLed0();
        switch (Events.ext) {
          case evxWifiOn:
            DV_println("wifi Connected");
            //setSyncProvider(getWWWTime);
            // setSyncInterval(6 * 3600);
            // lisen UDP 23423
            Serial.println("Listen broadcast");
            myUdp.begin();
            // restart MDNS
            MDNS.begin(Wifi.nodeName);
            MDNS.addService("http", "tcp", 80);
            Events.delayedPushMilli(checkWWW_DELAY, evCheckWWW);  // will send mail
            Events.delayedPushMilli(checkAPI_DELAY, evCheckAPI);
            break;
          case evxWifiOff:
            DV_println(F("wifi lost"));
            WWWOk = false;
            break;
        }

        writeHisto(Wifi.connected ? F("wifi Connected") : F("wifi lost"), WiFi.SSID());
      }

    case ev1Hz:
      {
        bootedSecondes++;

        // save current time in RTC memory
        //DV_println(currentTime);
        bNode.currentTime = now();
        //DV_println(currentTime);
        savedRTCmemory.actualTimestamp = bNode.currentTime;  // save time in RTC memory
        //Serial.println((uint)&savedRTCmemory.actualTimestamp,HEX);
        saveRTCmemory();
        //DV_println(savedRTCmemory.actualTimestamp);

        // If we are not connected we warn the user every 30 seconds that we need to update credential


        if (!Wifi.connected && second() % 30 == 15) {
          // every 30 sec
          Serial.print(F("module non connecté au Wifi local "));
          DV_println(WiFi.SSID());
          Serial.println(F("taper WIFI= pour configurer le Wifi"));
        }

        // au chagement de mois a partir 7H25 on envois le mail (un essais par heure)
        if (Wifi.connected && currentMonth != month() && hour() > 7 && minute() == 25 && second() == 0) {
          if (sendHistoTo(mailSendTo)) {
            if (currentMonth > 0) eraseHisto();
            currentMonth = month();
            writeHisto(F("Mail send ok"), mailSendTo);
          } else {
            writeHisto(F("Mail erreur"), mailSendTo);
          }
        }
      }
      break;



    case evCheckWWW:
      Serial.println("evCheckWWW");
      if (Wifi.connected) {
        if (WWWOk != (getWWWTime() > 0)) {
          WWWOk = !WWWOk;
          DV_println(WWWOk);
          writeHisto(WWWOk ? F("WWW Ok") : F("WWW Err"), "www.free.fr");
          if (WWWOk) {
            Serial.println("send a mail");
            bool sendOk = sendHistoTo(mailSendTo);
            if (sendOk) {
              //eraseHisto();
              writeHisto(F("Mail send ok"), mailSendTo);
            } else {
              writeHisto(F("Mail erreur"), mailSendTo);
            }
          }
        }
        Events.delayedPushMilli(checkWWW_DELAY, evCheckWWW);
      }
      break;

    case evCheckAPI:
      {
        Serial.println("evCheckAPI");
        if (!Wifi.connected) break;
        JSONVar jsonData = myDevices;
        jsonData["timeZone"] = bNode.timeZone;
        jsonData["timestamp"] = (double)bNode.currentTime;
        /*
          for (int N = 0; N < sondesNumber; N++) {
          //DV_println(sondesName[N]);
          //DV_println(sondesValue[N]);
          //jsonData[sondesName[N]] = sondesValue[N];
          jsonData[sondesName[N]] = myDevices["temperature"][sondesName[N]];
          }
        */
        String jsonStr = JSON.stringify(jsonData);
        if (APIOk != dialWithPHP(Wifi.nodeName, "timezone", jsonStr)) {
          APIOk = !APIOk;
          DV_println(APIOk);
          writeHisto(APIOk ? F("API Ok") : F("API Err"), "magnus2.frdev");
        }
        if (APIOk) {
          jsonData = JSON.parse(jsonStr);
          time_t aTimeZone = (const double)jsonData["timezone"];
          DV_println(aTimeZone);
          if (bNode.timeZone != aTimeZone) {
            String aStr = String(bNode.timeZone);
            aStr += " -> ";
            aStr += String(aTimeZone);
            writeHisto(F("Change TimeZone"), aStr);
            bNode.timeZone = aTimeZone;
            jobSetConfigInt("timezone", bNode.timeZone);
            // force recalculation of time
            setSyncProvider(getWWWTime);
            bNode.currentTime = now();
          }
        }
        Events.delayedPushMilli(checkAPI_DELAY, evCheckAPI);
      }
      break;

    // lecture des sondes temperature ds18b20
    case evDs18b20:
      {
        switch (Events.ext) {
          case evxDsRead:
            {
              DT_println("evxDsRead");
              int aSonde = mesDS18.current - 1;
              DV_println(aSonde);
              float aTemp = mesDS18.getTemperature();
              DTV_println("Temp:", aTemp);
              String aStr = sondesName[aSonde];
              DV_println(aStr);
              myDevices["temperature"][aStr] = String(aTemp).toDouble();  // trick to have 2 digit
              //myDevices["temperature"][aStr] = aTemp;
              String aTxt = "{\"temperature\":{\"";
              aTxt += sondesName[aSonde];
              aTxt += "\":";
              aTxt += String(aTemp);
              aTxt += "}}";

              // if (WiFiConnected) {
              DTV_println("BroadCast", aTxt);
              myUdp.broadcast(aTxt);
            }
            break;

          case evxDsError:
            TV_println("evxDsError", mesDS18.error);
            break;
        }


        //      }
      }

      break;




    case evBP0:

      switch (Events.ext) {
        case evxOn:
          //Led0.setMillisec(500, 50);
          jobUpdateLed0();
          BP0Multi++;
          Serial.println(F("BP0 Down"));
          if (BP0Multi > 1) {
            Serial.print(F("BP0 Multi ="));
            Serial.println(BP0Multi);
          }
          jobBcastSwitch(switchesName[0], 1);
          myDevices["switch"][switchesName[0]] = 1;
          break;
        case evxOff:
          jobUpdateLed0();
          Serial.println(F("BP0 Up"));
          jobBcastSwitch(switchesName[0], 0);
          myDevices["switch"][switchesName[0]] = 0;
          break;
        case evxLongOn:
          if (BP0Multi == 5) {
            Serial.println(F("RESET"));
            Events.push(doReset);
          }

          Serial.println(F("BP0 Long Down"));
          break;
        case evxLongOff:
          BP0Multi = 0;
          Serial.println(F("BP0 Long Up"));
          break;
      }
      break;


    case evBP1:
      switch (Events.ext) {
        case evxOn:
          Serial.println(F("BP1 On"));
          if (hallkey.length() and postInit and not shortBP1) {
            dialWithSonoffHall();
            Events.push(evStartAnim);
          }
          shortBP1 = true;
          break;
        case evxOff:
          Serial.println(F("BP1 Off"));
          if (hallkey.length() and postInit and not shortBP1) {
            dialWithSonoffHall();
            Events.push(evStartAnim);
          }
          shortBP1 = true;
          break;
        case evxLongOn:
          Serial.println(F("BP1 long On"));
          shortBP1 = false;
          jobBcastSwitch(switchesName[1], 1);
          myDevices["switch"][switchesName[1]] = 1;
          if (skey.length()) {
            Events.push(evStartAnim);
            if (postInit) dialWithSlack(F("Le lab est ouvert."));
          } else {
            Events.removeDelayEvent(evStartAnim);
          }
          break;
        case evxLongOff:
          Serial.println(F("BP1 Long Off"));
          shortBP1 = false;
          jobBcastSwitch(switchesName[1], 0);
          myDevices["switch"][switchesName[1]] = 0;
          Events.removeDelayEvent(evStartAnim);
          if (postInit and skey.length()) dialWithSlack(F("Le lab est fermé."));
          break;
      }
      break;
    case evTimeMasterSyncr:
      T_println(evTimeMasterSyncr);
      isTimeMaster = false;
      Events.delayedPushMilli(delayTimeMaster + (WiFi.localIP()[3] * 100), evTimeMasterGrab);
      //LedLife[1].setcolor((isMaster) ? rvb_blue : rvb_green, 30, 0, delayTimeMaster);
      jobUpdateLed0();  //synchro de la led de vie
      break;
    //deviens master en cas d'absence du master local
    case evTimeMasterGrab:
      T_println(evTimeMasterGrab);
      {
        String aStr = F("{\"event\":\"evMasterSyncr\"}");

        Events.delayedPushMilli(delayTimeMaster, evTimeMasterGrab);
        DT_println("evGrabMaster");
        //LedLife[1].setcolor((isMaster) ? rvb_blue : rvb_green, 30, 0, delayGrabMaster);
        jobUpdateLed0();
        if (timeStatus() != timeNotSet) myUdp.broadcastEvent(F("evTimeMasterSyncr"));
        if (!isTimeMaster) {
          isTimeMaster = true;
          //Events.push(evStartAnim1);
          //Events.push(evStartAnim3);
          //LedLife[1].setcolor(rvb_red, 30, 0, delayGrabMaster);
          // TODO placer cela dans le nodemcu de base
          // pour l'instant c'est je passe master je donne mon heure (si elle est valide ou presque :)  )

          // syncho de l'heure pour tout les bNodes
          static int8_t timeSyncrCnt = 0;
          if (timeSyncrCnt-- <= 0) {
            timeSyncrCnt = 15;
            if (timeStatus() != timeNotSet) {
              //{"TIME":{"timestamp":1707515817,"timezone":-1,"date":"2024-02-09 22:56:57"}}}
              String aStr = F("{\"TIME\":{\"timestamp\":");
              aStr += now() + bNode.timeZone * 3600;
              aStr += F(",\"timezone\":");
              aStr += bNode.timeZone;
              aStr += F("}}");
              myUdp.broadcast(aStr);
            }
          }
        }
      }
      break;

    case evUdp:
      if (Events.ext == evxUdpRxMessage) {
        DTV_print("from", myUdp.rxFrom);
        DTV_println("got an Event UDP", myUdp.rxJson);
        JSONVar rxJson = JSON.parse(myUdp.rxJson);

        //11:28:34.247 -> "UDP" => 'BetaporteHall', "DATA" => '{"action":"porte","close":true}'
        //11:28:11.712 -> "UDP" => 'BetaporteHall', "DATA" => '{"action":"porte","close":false}'
        //11:28:02.677 -> "UDP" => 'bNode03', "DATA" => '{"temperature":{"hallFond":12.06}}'
        //11:27:49.495 -> "UDP" => 'BetaporteHall', "DATA" => '{"action":"badge","userid":"Pierre H."}'
        //11:27:00.218 -> "UDP" => 'bLed256B', "DATA" => '{"event":"evMasterSyncr"}'
        // Received from BetaporteHall (10.11.12.52) TRAME1 245 : {"action":"badge","userid":"Pierre H."} Got 1 trames !!!
        // Received from BetaporteHall (10.11.12.52) TRAME1 246 : {"action":"porte","close":false}

        // event  "DATA" => '{"event":"evMasterSyncr"}'
        // evTimeMasterSyncr est toujour accepté
        //
        if (rxJson.hasOwnProperty("Event")) {
          String aStr = rxJson["Event"];
          DTV_println("external event", aStr);
          if (aStr.equals(F("evTimeMasterSyncr"))) {
            Events.delayedPushMilli(0, evTimeMasterSyncr);
          }
          break;
        }
        // action
        // Received from BetaporteHall (10.11.12.52) TRAME1 245 : {"action":"badge","userid":"Pierre H."} Got 1 trames !!!
        // Received from BetaporteHall (10.11.12.52) TRAME1 246 : {"action":"porte","close":false}
        if (rxJson.hasOwnProperty("action")) {
          String aStr = rxJson["action"];
          // if (JSON.typeof(rxJson2).equals("string")) {
          meshDevices[myUdp.rxFrom][aStr] = rxJson;
          DTV_println("external action", rxJson);
          break;
        }
        //INFO  (detection BOOT")
        JSONVar rxJson2 = rxJson["Info"];
        //DV_println(JSON.typeof(rxJson2));
        if ((year(bNode.currentTime) > 2000) and isTimeMaster and JSON.typeof(rxJson2).equals("string")) {
          //DV_println((String)rxJson2);
          if (((String)rxJson2).equals("Boot")) {  //it is a memeber who boot
            //{"TIME":{"timestamp":1707515817,"timezone":-1,"date":"2024-02-09 22:56:57"}}}
            String aStr = F("{\"TIME\":{\"timestamp\":");
            aStr += bNode.currentTime + bNode.timeZone * 3600;
            aStr += F(",\"timezone\":");
            aStr += bNode.timeZone;
            aStr += F("}}");
            myUdp.broadcast(aStr);
          }
        }

        //CMD
        //./bNodeCmd.pl bNode FREE -n
        //bNodeCmd.pl  V1.4
        //broadcast:BETA82	net234	{"CMD":{"bNode":"FREE"}}
        rxJson2 = rxJson["CMD"];
        if (JSON.typeof(rxJson2).equals("object")) {
          String dest = rxJson2.keys()[0];  //<nodename called>
          // Les CMD acceptée doivent etre adressé a ce module
          if (dest.equals("ALL") or (dest.length() > 3 and Wifi.nodeName.startsWith(dest))) {
            String aCmd = rxJson2[dest];
            aCmd.trim();
            DV_println(aCmd);
            if (aCmd.startsWith("NODE=") and !Wifi.nodeName.equals(dest)) break;  // NODE= not allowed on aliases
            if (aCmd.length()) Keyboard.setInputString(aCmd);
          } else {
            DTV_println("CMD not for me.", dest);
          }
          break;
        }



        // temperature
        rxJson2 = rxJson["temperature"];
        if (JSON.typeof(rxJson2).equals("object")) {
          String aName = rxJson2.keys()[0];
          //DV_println(aName);
          double aValue = rxJson2[aName];
          //DV_println(aValue);
          meshDevices[myUdp.rxFrom]["temperature"][aName] = aValue;
          DTV_println("grab temperature", aValue);
          break;
        }

        //{"switch":{"FLASH":0}}
        // temperature
        rxJson2 = rxJson["switch"];
        if (JSON.typeof(rxJson2).equals("object")) {
          String aName = rxJson2.keys()[0];
          //DV_println(aName);
          double aValue = rxJson2[aName];
          //DV_println(aValue);
          meshDevices[myUdp.rxFrom]["switch"][aName] = aValue;
          DTV_println("grab switch", aValue);
          break;
        }

        //TIME  TODO: a finir time
        rxJson2 = rxJson["TIME"];
        if (JSON.typeof(rxJson2).equals("object")) {
          int aTimeZone = (int)rxJson2["timezone"];
          DV_println(aTimeZone);
          if (aTimeZone != bNode.timeZone) {
            //             writeHisto( F("Old TimeZone"), String(timeZone) );
            bNode.timeZone = aTimeZone;
            jobSetConfigInt("timezone", bNode.timeZone);
          }
          time_t aTime = (unsigned long)rxJson2["timestamp"] - (bNode.timeZone * 3600);
          DV_println(niceDisplayTime(bNode.currentTime, true));
          DV_println(niceDisplayTime(aTime, true));
          int delta = aTime - bNode.currentTime;
          DV_println(delta);
          writeHisto("time adjust", String(delta));
          if (abs(delta) < 10) {
            adjustTime(delta);
            bNode.currentTime = now();
          } else {
            bNode.currentTime = aTime;
            setTime(aTime);  // this will rearm the call to the
          }
          setSyncInterval(6 * 3600);
          //DV_println(currentTime);

          //DV_println(niceDisplayTime(currentTime, true));
        }
      }
      break;

    case doReset:
      helperReset();
      break;


      //    case evInChar: {
      //        if (MyDebug.trackTime < 2) {
      //          char aChar = Keyboard.inputChar;
      //          if (isPrintable(aChar)) {
      //            DV_println(aChar);
      //          } else {
      //            DV_println(int(aChar));
      //          }
      //        }
      //        switch (toupper(Keyboard.inputChar))
      //        {
      //          case '0': delay(10); break;
      //          case '1': delay(100); break;
      //          case '2': delay(200); break;
      //          case '3': delay(300); break;
      //          case '4': delay(400); break;
      //          case '5': delay(500); break;
      //
      //        }
      //      }
      //      break;
      //


    case evInString:
      //DV_println(Keyboard.inputString);
      if (Keyboard.inputString.startsWith(F("?"))) {
        Serial.println(F("Liste des commandes"));
        Serial.println(F("NODE=nodename (nom du module)"));
        Serial.println(F("WIFI=ssid,paswword"));
        Serial.println(F("MAILTO=adresse@mail    (mail du destinataire)"));
        Serial.println(F("MAILFROM=adresse@mail  (mail emetteur 'NODE' sera remplacé par nodename)"));
        Serial.println(F("SMTPSERV=mail.mon.fai,login,password  (SMTP serveur et credential) "));
        Serial.println(F("SONDENAMES=name1,name2...."));
        Serial.println(F("SWITCHENAMES=name1,name2...."));
        Serial.println(F("RAZCONF      (efface la config sauf le WiFi)"));
        Serial.println(F("MAIL         (envois un mail de test)"));
        Serial.println(F("API          (envois une commande API timezone)"));
        Serial.println(F("BCAST        (envoi un broadcast)"));
      }

      if (Keyboard.inputString.startsWith(F("NODE="))) {
        Serial.println(F("SETUP NODENAME : 'NODE= nodename'  ( this will reset)"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        aStr.replace(" ", "_");
        aStr.trim();

        if (aStr != "") {
          String nodeName = aStr;
          DV_println(nodeName);
          jobSetConfigStr(F("nodename"), nodeName);
          delay(1000);
          helperReset();
        }
      }


      if (Keyboard.inputString.startsWith(F("WIFI="))) {
        Serial.println(F("SETUP WIFI : 'WIFI= WifiName, password"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        String ssid = grabFromStringUntil(aStr, ',');
        ssid.trim();
        DV_println(ssid);
        if (ssid != "") {
          String pass = aStr;
          pass.trim();
          DV_println(pass);
          bool result = WiFi.begin(ssid, pass);
          //WiFi.setAutoConnect(true);
          DV_println(WiFi.getAutoConnect());
          Serial.print(F("WiFi begin "));
          DV_println(result);
        }
      }
      if (Keyboard.inputString.startsWith(F("MAILTO="))) {
        Serial.println(F("SETUP mail to  : 'MAILTO=monAdresseMail@monfai'"));
        String aMail = Keyboard.inputString;
        grabFromStringUntil(aMail, '=');
        aMail.trim();
        DV_println(aMail);
        if (aMail != "") {
          jobSetConfigStr(F("mailto"), aMail);
          mailSendTo = aMail;
        }
      }

      if (Keyboard.inputString.equals("1")) {

        ledFixe1.setcolor(rvb_red, 80, 0, 5000);
      }

      if (Keyboard.inputString.equals("2")) {

        ledFixe2.setcolor(rvb_red, 80, 0, 5000);
      }
      if (Keyboard.inputString.equals("3")) {

        ledFixe3.setcolor(rvb_red, 80, 0, 5000);
      }
      if (Keyboard.inputString.equals("4")) {

        ledFixe1.setcolor(rvb_white, 100, 0, 0);
      }

      /***
        if (Keyboard.inputString.equals(F("WIFIOFF"))) {
        Serial.println("setWiFiMode(WiFi_OFF)");
        WiFi.forceSleepWake();
        delay(1);

        WiFi.mode(WIFI_OFF);
        }

        if (Keyboard.inputString.equals(F("WIFISTA"))) {
        Serial.println("setWiFiMode(WiFi_STA)");
        WiFi.forceSleepWake();
        delay(1);
        WiFi.mode(WIFI_STA);
        WiFi.begin();
        if (WiFi.waitForConnectResult() != WL_CONNECTED) Serial.println(F("WIFI NOT CONNECTED"));
        }
      **/



      if (Keyboard.inputString.startsWith(F("SLACKPOST="))) {
        Serial.println(F("SLACKPOST= message"));
        String aMsg = Keyboard.inputString;
        grabFromStringUntil(aMsg, '=');
        aMsg.trim();
        DV_println(aMsg);

        if (aMsg != "") dialWithSlack(aMsg);
      }


      if (Keyboard.inputString.startsWith(F("MAILFROM="))) {
        Serial.println(F("SETUP mail to  : 'MAILFROM=NODE@monfai'"));
        String aMail = Keyboard.inputString;
        grabFromStringUntil(aMail, '=');
        aMail.trim();
        DV_println(aMail);
        if (aMail != "") jobSetConfigStr(F("mailfrom"), aMail);
      }





      if (Keyboard.inputString.startsWith(F("SMTPSERV="))) {
        Serial.println(F("SETUP smtp serveur : 'SMTPSERV=smtp.mon_serveur.xx,login,pass'"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        String aSmtp = grabFromStringUntil(aStr, ',');
        aSmtp.trim();
        String aLogin = grabFromStringUntil(aStr, ',');
        aLogin.trim();
        String aPass = aStr;
        aPass.trim();
        DV_println(aSmtp);
        if (aSmtp != "") {
          jobSetConfigStr(F("smtpserver"), aSmtp);
          jobSetConfigStr(F("smtplogin"), aLogin);
          jobSetConfigStr(F("smtppass"), aPass);
        }
      }

      if (Keyboard.inputString.startsWith(F("SONDENAMES="))) {
        Serial.println(F("SETUP sonde name : 'SONDENAMES=name1,name2....'"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');

        aStr.replace("#", "_");
        aStr.trim();

        jobSetConfigStr(F("sondename"), aStr);
        jobGetSondeName();
      }

      if (Keyboard.inputString.startsWith(F("SWITCHNAMES="))) {
        Serial.println(F("SETUP sonde name : 'SWITCHNAMES=name1,name2....'"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');

        aStr.replace("#", "_");
        aStr.trim();

        jobSetConfigStr(F("switchename"), aStr);
        jobGetSwitcheName();
      }


      if (Keyboard.inputString.startsWith(F("SKEY="))) {
        Serial.println(F("SETUP slack key : 'SKEY=slack key'"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        aStr.trim();
        jobSetConfigStr(F("skey"), aStr);
        skey = aStr;
      }

      if (Keyboard.inputString.startsWith(F("HALLKEY="))) {
        Serial.println(F("SETUP hall toggle key : 'HALLKEY=hall toggle key'"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        aStr.trim();
        jobSetConfigStr(F("hallkey"), aStr);
        hallkey = aStr;
      }

      if (Keyboard.inputString.equals(F("RAZCONF"))) {
        Serial.println(F("RAZCONF this will reset"));
        eraseConfig();
        delay(1000);
        helperReset();
      }



      if (Keyboard.inputString.equals(F("RESET"))) {
        Serial.println(F("RESET"));
        Events.push(doReset);
      }
      if (Keyboard.inputString.equals(F("FREE"))) {
        String aStr = F("Ram=");
        aStr += String(helperFreeRam());
        aStr += F(",APP=" APP_NAME);
        Serial.println(aStr);
        myUdp.broadcastInfo(aStr);
      }

      if (Keyboard.inputString.equals(F("CLEAN"))) {
        T_println("Cleanup");
        DV_println(helperFreeRam());
        {
          JSONVar meshDevices2 = meshDevices;
          meshDevices = undefined;
          JSONVar myDevices2 = myDevices;
          myDevices = undefined;

          myDevices = myDevices2;
          meshDevices = meshDevices2;
        }
        DV_println(helperFreeRam());
      }

      //num {timeNotSet, timeNeedsSync, timeSet
      if (Keyboard.inputString.equals(F("TIME?"))) {
        String aStr = F("mydate=");
        aStr += niceDisplayTime(bNode.currentTime, true);
        aStr += F(" timeZone=");
        aStr += bNode.timeZone;
        aStr += F(" timeStatus=");
        aStr += timeStatus();
        aStr += F(" webClockDelta=");
        aStr += webClockDelta;
        aStr += F("s  webClockLastTry=");
        aStr += niceDisplayTime(webClockLastTry, true);


        Serial.println(aStr);
        myUdp.broadcastInfo(aStr);
      }



      if (Keyboard.inputString.equals(F("INFO"))) {
        String aStr = F("node=");
        aStr += Wifi.nodeName;
        aStr += F(" isTimeMaster=");
        aStr += String(isTimeMaster);
        aStr += F(" CPU=");
        aStr += String(Events._percentCPU);
        aStr += F("% ack=");
        aStr += String(myUdp.ackPercent);
        aStr += F("%  booted=");
        aStr += niceDisplayDelay(bootedSecondes);


        //ledLifeVisible = true;
        //Events.delayedPushMilli(5 * 60 * 1000, evHideLedLife);
        myUdp.broadcastInfo(aStr);
        DV_println(aStr)
      }
      if (Keyboard.inputString.equals(F("HIST"))) {
        DT_println("HIST");
        printHisto();
      }
      if (Keyboard.inputString.equals(F("CONF"))) {
        jobShowConfig();
      }
      if (Keyboard.inputString.equals(F("ERASEHISTO"))) {
        eraseHisto();
      }
      if (Keyboard.inputString.equals(F("RTC"))) {
        V_print(getRTCMemory());
        V_println(niceDisplayTime(savedRTCmemory.actualTimestamp, true));
      }

      if (Keyboard.inputString.equals(F("MAIL"))) {
        bool mailHisto = sendHistoTo(mailSendTo);
        DV_println(mailHisto);
      }

      if (Keyboard.inputString.equals("S")) {
        sleepOk = !sleepOk;
        DV_println(sleepOk);
      }
      if (Keyboard.inputString.equals("API")) {
        String jsonData = "";
        bool dialWPHP = dialWithPHP(Wifi.nodeName, "timezone", jsonData);
        DV_println(jsonData);
        DV_println(dialWPHP);
      }

      if (Keyboard.inputString.equals("CHKAPI")) {
        Events.push(evCheckAPI);
        T_println("Force check API");
      }

      if (Keyboard.inputString.equals("OTA")) {
        Events.push(evOta, evxOn);
        T_println("Start OTA");
      }



      if (Keyboard.inputString.equals("BCAST")) {
        myUdp.broadcastInfo("test broadcast");
        T_println("test broadcast");
      }

      if (Keyboard.inputString.equals("ANIMON")) {
        Events.push(evStartAnim);
        T_println("ANIMON");
      }
      if (Keyboard.inputString.equals("ANIMOFF")) {
        Events.removeDelayEvent(evStartAnim);
        T_println("ANIMOFF");
      }

      if (Keyboard.inputString.equals("WWWCHECK")) {

        TV_println("WWWCHECK", niceDisplayTime(getWWWTime(), true));
      }

      if (Keyboard.inputString.equals("DATEHISTO")) {

        TV_println("DATEHISTO", niceDisplayTime(jobGetHistoTime(), true));
      }
      if (Keyboard.inputString.equals("SETDATEHISTO")) {
        jobSetHistoTime();
        TV_println("DATEHISTO", niceDisplayTime(jobGetHistoTime(), true));
      }

      break;
  }
}

// fatal error
// flash led0 same number as error 10 time then reset
//
void fatalError(const uint8_t error) {
  Serial.print(F("Fatal error "));
  Serial.println(error);


  // display error on LED_BUILTIN
  for (uint8_t N = 1; N <= 5; N++) {
    for (uint8_t N1 = 1; N1 <= error; N1++) {
      delay(150);
      Led0.setOn(true);
      beep(988, 100);
      delay(150);
      Led0.setOn(false);
    }
    delay(500);
  }
  delay(2000);
  helperReset();
}


void beep(const uint16_t frequence, const uint16_t duree) {
  tone(BEEP_PIN, frequence, duree);
}
