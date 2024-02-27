/*************************************************
 *************************************************
    Sketch checkMyBox.ino   check and report  a box and the wifi connectivity
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
//25/02/2024  V1.0a ajout  webserveur pour faire une api  via un handlerHttp


#define APP_NAME "checkMyBox V1.4"

#include <ArduinoOTA.h>
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
  evPostInit,
  evRefreshLed,
  evStartAnim,  //Allumage Avec l'animation
  evNextStep,   //etape suivante dans l'animation
  evDs18x20,    // event interne DS18B80
  evSonde1,     // event sonde1
  evSonde2,     // event sonde2
  evSonde3,
  evSondeMAX = evDs18x20 + 20,  //
  evStartOta,
  evStopOta,
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
const uint32_t DS18X_DELAY = (5 * 60 * 1000L);  // lecture des sondes toute les 5 minutes

// instance betaEvent

//  une instance "MyEvents" avec un poussoir "BP0" une LED "Led0" un clavier "Keyboard"
//  MyBP0 genere un evenement evBP0 a chaque pression le poussoir connecté sur D2
//  Led0 genere un evenement evLed0 a chaque clignotement de la led precablée sur la platine
//  Keyboard genere un evenement evChar a char caractere recu et un evenement evString a chaque ligne recue
//  MyDebug permet sur reception d'un "T" sur l'entrée Serial d'afficher les infos de charge du CPU
//#define DEBUG_ON
#include "ESP8266.H"
#include <BetaEvents32.h>
#define NOT_A_DATE_YEAR 2000


// BP0 est créé automatiquement par BetaEvent.h
evHandlerButton BP1(evBP1, BP1_PIN);  // pousssoir externe 

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>


// Sondes temperatures : DS18B20
//instance du bus OneWire dedié aux DS18B20
#include "evHandlerDS18x20.h"

evHandlerDS18x20 ds18x(ONEWIRE_PIN, DS18X_DELAY);


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




// littleFS
#include <LittleFS.h>  //Include File System Headers
#define MyLittleFS LittleFS

//WiFI
#include <ESP8266WiFi.h>
//#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>
//WiFiClientSecure client;

// rtc memory to keep date
//struct __attribute__((packed))
struct {
  // all these values are keep in RTC RAM
  uint8_t crc8;            // CRC for savedRTCmemory
  time_t actualTimestamp;  // time stamp restored on next boot Should be update in the loop() with setActualTimestamp
} savedRTCmemory;


// Variable d'application locale
String nodeName = "NODE_NAME";  // nom de  la device (a configurer avec NODE=)"

time_t currentTime;
time_t webClockLastTry = 0;
uint32_t bootedSecondes = 0;
int webClockDelta = 0;
int currentMonth = -1;

bool WiFiConnected = false;
const int delayTimeMaster = 60 * 1000;  // par defaut toute les Minutes
bool isTimeMaster = false;
bool configErr = false;
bool WWWOk = false;
bool APIOk = false;
bool sleepOk = true;
int multi = 0;         // nombre de clic rapide
bool configOk = true;  // global used by getConfig...
const byte postInitDelay = 10;
bool postInit = false;          // true postInitDelay secondes apres le boot (limitation des messages Slack)
String mailSendTo;              // mail to send email
int8_t timeZone = 0;            //-2;  //les heures sont toutes en localtimes (par defaut hivers france)
int8_t sondesNumber = 0;        // nombre de sonde
String sondesName[MAXDS18x20];  // noms des sondes
float sondesValue[MAXDS18x20];  // valeur des sondes
const int8_t switchesNumber = 2;
String switchesName[switchesNumber];
int8_t displayStep = 0;
// init UDP
#include "evHandlerUdp.h"
const unsigned int localUdpPort = 23423;  // local port to listen on
evHandlerUdp myUdp(evUdp, localUdpPort, nodeName);
e_rvb ledLifeColor = rvb_white;
JSONVar myDevices, meshDevices;

#include "evHandlerHttp.h"
evHandlerHttp myHttp(evHttp);

void setup() {
  enableWiFiAtBootTime();  // mendatory for autoconnect WiFi with ESP8266 kernel 3.0
  //Serial.begin(115200);
  //Serial.println(F("\r\n\n" APP_NAME));
  //DV_println(sizeof(stdEvent_t));
  //delay(3000);
  // Start instance
  Events.begin();
  Serial.println(F("\r\n\n" APP_NAME));
  DV_println(WiFi.getMode());

  //  normaly not needed
  if (WiFi.getMode() != WIFI_STA) {
    Serial.println(F("!!! Force WiFi to STA mode !!!"));
    WiFi.mode(WIFI_STA);
    WiFi.setAutoConnect(true);
    WiFi.begin();
    //WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  // System de fichier
  if (!MyLittleFS.begin()) {
    Serial.println(F("erreur MyLittleFS"));
    fatalError(3);
  }

  // recuperation de l'heure dans la static ram de l'ESP
  if (!getRTCMemory()) {
    savedRTCmemory.actualTimestamp = 0;
  }
  // little trick to leave timeStatus to timeNotSet
  // TODO: see with https://github.com/PaulStoffregen/Time to find a way to say timeNeedsSync
  adjustTime(savedRTCmemory.actualTimestamp);
  currentTime = savedRTCmemory.actualTimestamp;

  // recuperation de la config dans config.json
  nodeName = jobGetConfigStr(F("nodename"));
  if (nodeName == "") {
    Serial.println(F("!!! Configurer le nom de la device avec 'NODE=nodename' !!!"));
    configErr = true;
    nodeName = "checkMyBox_";
    nodeName += WiFi.macAddress().substring(12, 14);
    nodeName += WiFi.macAddress().substring(15, 17);
  }
  DV_println(nodeName);

  // recuperation de la timezone dans la config
  timeZone = jobGetConfigInt(F("timezone"));
  if (!configOk) {
    timeZone = -2;  // par defaut France hivers
    jobSetConfigInt(F("timezone"), timeZone);
    Serial.println(F("!!! timezone !!!"));
  }
  DV_println(timeZone);

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
  DV_println(Events.freeRam());






  // Recuperation du nom des sondes
  sondesNumber = ds18x.getNumberOfDevices();
  if (!sondesNumber) {
    DV_println(sondesNumber);
    delay(500);
    sondesNumber = ds18x.getNumberOfDevices();
  }
  if (sondesNumber > MAXDS18x20) sondesNumber = MAXDS18x20;
  DV_println(sondesNumber);
  jobGetSondeName();

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

  Serial.println("Wait a for wifi");
  for (int N = 0; N < 50; N++) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(100);
  }


  jobUpdateLed0();

  Serial.println("Bonjour ....");
  Serial.println("Tapez '?' pour avoir la liste des commandes");
  DV_println(LED_BUILTIN);
}

byte BP0Multi = 0;


//String niceDisplayTime(const time_t time, bool full = false);
bool buildApiAnswer(JSONVar& answer, const String& action, const String& value) {
  DTV_print("api call", action);
  DV_println(value);
  if (!action.length()) {
    answer["node"]["name"] = nodeName;
    answer["node"]["date"] = niceDisplayTime(currentTime, true);
    answer["node"]["booted"] = niceDisplayDelay(bootedSecondes);
    answer["devices"] = meshDevices;
    answer["devices"][nodeName] = myDevices;
    ServerHttp.sendHeader("refresh","60", true);
    return true;
  }
  if (action.equals("CMD") and value.length()) {
    Keyboard.setInputString(value);
    answer["cmd"] = value;
    DTV_println("API CMD", value);
    ServerHttp.sendHeader("refresh","5;url=api.json", true);
    return true;
  }

  //liste les capteurs du type 'action'    api.Json?temperature listera les capteurs temperature
  bool matched = false;
  if (JSON.typeof(myDevices[action]).equals("object")) {
    DV_println(myDevices[action]);
    JSONVar aJson = myDevices[action];
    answer[nodeName][action] = aJson;
    matched = true;
  }
  // recherche dans le mesh
  JSONVar keys = meshDevices.keys();
  for (int i = 0; i < keys.length(); i++) {
    String aKey = keys[i];
    DV_println(aKey);
    if (JSON.typeof(meshDevices[aKey][action]).equals("object")) {
      JSONVar aJson = meshDevices[aKey][action];
      answer[aKey][action] = aJson;
      matched = true;
    }
  }
  if (matched){
    ServerHttp.sendHeader("refresh","60", true);
    return true;
  }


  return false;
}

void loop() {
  MDNS.update();
  ArduinoOTA.handle();
  Events.get(sleepOk);
  Events.handle();
  switch (Events.code) {
    case evInit:
      {
        Serial.println("Init");
        String aStr = nodeName;
        aStr += ' ';
        aStr += APP_NAME;

        writeHisto(F("Boot"), aStr);
        Events.delayedPushMilli(3000, evStartAnim);
        Events.delayedPushMilli(2500, evRefreshLed);
        Events.delayedPushMilli(postInitDelay * 1000, evPostInit);
        Events.delayedPushMilli(5000, evStartOta);
        myUdp.broadcastInfo("Boot");
        jobUpdateLed0();
      }
      break;

    case evPostInit:
      postInit = true;
      jobUpdateLed0();
      T_println("PostInit done");


      break;


    case evStopOta:
      Serial.println("Stop OTA");
      myUdp.broadcastInfo("Stop OTA");
      ArduinoOTA.end();
      writeHisto(F("Stop OTA"), nodeName);
      // but restart MDNS
      MDNS.begin(nodeName);
      MDNS.addService("http", "tcp", 80);
      break;

    case evStartOta:
      {
        // start OTA
        String deviceName = nodeName;  // "ESP_";

        ArduinoOTA.setHostname(deviceName.c_str());
        ArduinoOTA.begin(true);                               //MDNS is handled in main loop
        Events.delayedPushMilli(1000L * 15 * 60, evStopOta);  // stop OTA dans 15 Min

        //MDNS.update();
        Serial.print("OTA on '");
        Serial.print(deviceName);
        Serial.println("' started.");
        Serial.print("SSID:");
        Serial.println(WiFi.SSID());
        myUdp.broadcastInfo("start OTA");
        //end start OTA
      }
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
        String newDateTime = niceDisplayTime(currentTime, true);
        DV_println(newDateTime);
        writeHisto(F("newDateTime"), String(webClockDelta));
        //webClockDelta = 0;
      }
      break;

    case ev1Hz:
      {
        bootedSecondes++;
        //jobCheckWifi();

        // check for connection to local WiFi  1 fois par seconde c'est suffisant
        static uint8_t oldWiFiStatus = 99;
        uint8_t WiFiStatus = WiFi.status();
        if (oldWiFiStatus != WiFiStatus) {
          oldWiFiStatus = WiFiStatus;
          DV_println(WiFiStatus);
          //    WL_IDLE_STATUS      = 0,
          //    WL_NO_SSID_AVAIL    = 1,
          //    WL_SCAN_COMPLETED   = 2,
          //    WL_CONNECTED        = 3,
          //    WL_CONNECT_FAILED   = 4,
          //    WL_CONNECTION_LOST  = 5,
          //    WL_DISCONNECTED     = 6
          //    7: WL_AP_LISTENING
          //    8: WL_AP_CONNECTED

          WiFiConnected = (WiFiStatus == WL_CONNECTED);
          static bool wasConnected = false;
          if (wasConnected != WiFiConnected) {
            wasConnected = WiFiConnected;
            jobUpdateLed0();
            //Led0.setMillisec(WiFiConnected ? 2000 : 500, 5);
            if (WiFiConnected) {
              setSyncProvider(getWebTime);
              setSyncInterval(6 * 3600);
              // lisen UDP 23423
              Serial.println("Listen broadcast");
              myUdp.begin();
              // restart MDNS
              MDNS.begin(nodeName);
              MDNS.addService("http", "tcp", 80);
              Events.delayedPushMilli(checkWWW_DELAY, evCheckWWW);  // will send mail
              Events.delayedPushMilli(checkAPI_DELAY, evCheckAPI);
            } else {
              WWWOk = false;
            }
            DV_println(WiFiConnected);
            writeHisto(WiFiConnected ? F("wifi Connected") : F("wifi lost"), WiFi.SSID());
          }
        }

        // save current time in RTC memory
        currentTime = now();
        savedRTCmemory.actualTimestamp = currentTime;  // save time in RTC memory
        saveRTCmemory();

        // If we are not connected we warn the user every 30 seconds that we need to update credential


        if (!WiFiConnected && second() % 30 == 15) {
          // every 30 sec
          Serial.print(F("module non connecté au Wifi local "));
          DV_println(WiFi.SSID());
          Serial.println(F("taper WIFI= pour configurer le Wifi"));
        }

        // au chagement de mois a partir 7H25 on envois le mail (un essais par heure)
        if (WiFiConnected && currentMonth != month() && hour() > 7 && minute() == 25 && second() == 0) {
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
      if (WiFiConnected) {
        if (WWWOk != (getWebTime() > 0)) {
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
        if (!WiFiConnected) break;
        JSONVar jsonData;
        jsonData["timeZone"] = timeZone;
        jsonData["timestamp"] = (double)currentTime;
        for (int N = 0; N < sondesNumber; N++) {
          //DV_println(sondesName[N]);
          //DV_println(sondesValue[N]);
          jsonData[sondesName[N]] = sondesValue[N];
        }
        String jsonStr = JSON.stringify(jsonData);
        if (APIOk != dialWithPHP(nodeName, "timezone", jsonStr)) {
          APIOk = !APIOk;
          DV_println(APIOk);
          writeHisto(APIOk ? F("API Ok") : F("API Err"), "magnus2.frdev");
        }
        if (APIOk) {
          jsonData = JSON.parse(jsonStr);
          time_t aTimeZone = (const double)jsonData["timezone"];
          DV_println(aTimeZone);
          if (aTimeZone != timeZone) {
            String aStr = String(timeZone);
            aStr += " -> ";
            aStr += String(aTimeZone);
            writeHisto(F("Change TimeZone"), aStr);
            timeZone = aTimeZone;
            jobSetConfigInt("timezone", timeZone);
            // force recalculation of time
            setSyncProvider(getWebTime);
            currentTime = now();
          }
        }
        Events.delayedPushMilli(checkAPI_DELAY, evCheckAPI);
      }
      break;

    // lecture des sondes
    case evSonde1 ... evSondeMAX:
      {
        int aSonde = Events.code - evSonde1;
        DV_println(aSonde);
        sondesValue[aSonde] = Events.intExt / 100.0;
        DV_println(Events.intExt);
        String aStr = sondesName[aSonde];
        DV_println(aStr);
        myDevices["temperature"][aStr] = sondesValue[aSonde];






        Serial.print(sondesName[aSonde]);
        Serial.print(" : ");
        Serial.println(sondesValue[aSonde]);
        String aTxt = "{\"temperature\":{\"";
        aTxt += sondesName[aSonde];
        aTxt += "\":";
        aTxt += String(sondesValue[aSonde]);
        aTxt += "}}";

        // if (WiFiConnected) {
        DTV_println("BroadCast", aTxt);
        myUdp.broadcast(aTxt);
        //      }
      }

      break;

    // lecture des sondes


    case evDs18x20:
      {
        if (Events.ext == evxDsRead) {
          //          if (ds18x.error) {
          //            DV_println(ds18x.error);
          //            // TODO: gerer le moErreur;
          //            break;
          //          };
          //          DV_println(ds18x.current);
          //          DV_println(ds18x.celsius());
        }
        if (Events.ext == evxDsError) {
          Serial.print(F("Erreur Sonde N°"));
          Serial.print(ds18x.current);
          Serial.print(F(" : "));
          Serial.println(ds18x.error);
        }
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
        case evxLongOn:
          Serial.println(F("BP0 long On"));
          jobBcastSwitch(switchesName[1], 1);
          myDevices["switch"][switchesName[1]] = 1;
          Events.push(evStartAnim);
          if (postInit) dialWithSlack(F("Le lab est ouvert."));
          break;
        case evxLongOff:
          Serial.println(F("BP0 Long Off"));
          jobBcastSwitch(switchesName[1], 0);
          myDevices["switch"][switchesName[1]] = 0;
          Events.removeDelayEvent(evStartAnim);
          if (postInit) dialWithSlack(F("Le lab est fermé."));
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
              aStr += now() + timeZone * 3600;
              aStr += F(",\"timezone\":");
              aStr += timeZone;
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
        if ((year(currentTime) > 2000) and isTimeMaster and JSON.typeof(rxJson2).equals("string")) {
          //DV_println((String)rxJson2);
          if (((String)rxJson2).equals("Boot")) {  //it is a memeber who boot
            //{"TIME":{"timestamp":1707515817,"timezone":-1,"date":"2024-02-09 22:56:57"}}}
            String aStr = F("{\"TIME\":{\"timestamp\":");
            aStr += currentTime + timeZone * 3600;
            aStr += F(",\"timezone\":");
            aStr += timeZone;
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
          if (dest.equals("ALL") or (dest.length() > 3 and nodeName.startsWith(dest))) {
            String aCmd = rxJson2[dest];
            aCmd.trim();
            DV_println(aCmd);
            if (aCmd.startsWith("NODE=") and !nodeName.equals(dest)) break;  // NODE= not allowed on aliases
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
          if (aTimeZone != timeZone) {
            //             writeHisto( F("Old TimeZone"), String(timeZone) );
            timeZone = aTimeZone;
            jobSetConfigInt("timezone", timeZone);
          }
          time_t aTime = (unsigned long)rxJson2["timestamp"] - (timeZone * 3600);
          DV_println(niceDisplayTime(currentTime, true));
          DV_println(niceDisplayTime(aTime, true));
          int delta = aTime - currentTime;
          DV_println(delta);
          //if (abs(delta) < 5000) {
          //  adjustTime(delta);
          //  currentTime = now();
          //} else {
          //  currentTime = aTime;
          setTime(aTime);  // this will rearm the call to the
          //}
          //DV_println(currentTime);

          //DV_println(niceDisplayTime(currentTime, true));
        }
      }
      break;

    case doReset:
      Events.reset();
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
          nodeName = aStr;
          DV_println(nodeName);
          jobSetConfigStr(F("nodename"), nodeName);
          delay(1000);
          Events.reset();
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

        aStr.replace("#", "");
        aStr.trim();

        jobSetConfigStr(F("sondename"), aStr);
        jobGetSondeName();
      }

      if (Keyboard.inputString.startsWith(F("SWITCHNAMES="))) {
        Serial.println(F("SETUP sonde name : 'SWITCHNAMES=name1,name2....'"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');

        aStr.replace("#", "");
        aStr.trim();

        jobSetConfigStr(F("switchename"), aStr);
        jobGetSwitcheName();
      }


      if (Keyboard.inputString.startsWith(F("SKEY="))) {
        Serial.println(F("SETUP sonde name : 'SKEY=slack key'"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        aStr.trim();

        jobSetConfigStr(F("skey"), aStr);
      }

      if (Keyboard.inputString.equals(F("RAZCONF"))) {
        Serial.println(F("RAZCONF this will reset"));
        eraseConfig();
        delay(1000);
        Events.reset();
      }



      if (Keyboard.inputString.equals(F("RESET"))) {
        Serial.println(F("RESET"));
        Events.push(doReset);
      }
      if (Keyboard.inputString.equals(F("FREE"))) {
        String aStr = F("Ram=");
        aStr += String(Events.freeRam());
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
        aStr += niceDisplayTime(currentTime, true);
        aStr += F(" timeZone=");
        aStr += now();
        aStr += F(" timeStatus=");
        aStr += timeStatus();
        aStr += F(" webClockDelta=");
        aStr += webClockDelta;
        aStr += F("ms  webClockLastTry=");
        aStr += niceDisplayTime(webClockLastTry, true);


        Serial.println(aStr);
        myUdp.broadcastInfo(aStr);
      }



      if (Keyboard.inputString.equals(F("INFO"))) {
        String aStr = F("node=");
        aStr += nodeName;
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
        printHisto();
      }
      if (Keyboard.inputString.equals(F("CONF"))) {
        jobShowConfig();
      }
      if (Keyboard.inputString.equals(F("ERASEHISTO"))) {
        eraseHisto();
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
        bool dialWPHP = dialWithPHP(nodeName, "timezone", jsonData);
        DV_println(jsonData);
        DV_println(dialWPHP);
      }

      if (Keyboard.inputString.equals("CHKAPI")) {
        Events.push(evCheckAPI);
        T_println("Force check API");
      }

      if (Keyboard.inputString.equals("OTA")) {
        Events.push(evStartOta);
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
  Events.reset();
}


void beep(const uint16_t frequence, const uint16_t duree) {
  tone(BEEP_PIN, frequence, duree);
}
