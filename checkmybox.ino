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
   TODO: replace webClock with ntptime ?
   TODO: add a 1wire temp sensor to send to API ?
    V1.2  (27/10/2021)
    adjust for Betaevent 2.2
    TODO: make a default nodename buid upon mac adresse
    TODO: bug   lost node name on first init
    FIXED: bug   MAILTO not updated in global when changed
    final version with arduino 32bit time_t
    V1.3  (19/10/2023)
    simplification de evHandlerDS18x20 pour gerer de multiple sondes
    ajout de l'OTA actif 5 minutes apres le boot




 *************************************************/

#define APP_NAME "checkMyBox V1.3.B2"
#include <ArduinoOTA.h>
static_assert(sizeof(time_t) == 8, "This version works with time_t 32bit  moveto ESP8266 kernel 3.0");


/* Evenements du Manager (voir EventsManager.h)
  evNill = 0,      // No event  about 1 every milisecond but do not use them for delay Use delayedPush(delay,event)
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
  evLed0,
  evDs18x20, // event interne DS18B80
  evSonde1,  // event sonde1
  evSonde2,  // event sonde2
  evSonde3,
  evSondeMAX = evDs18x20 + 20,  //
  evStopOta,
  evCheckWWW,
  evCheckAPI,
  evUdp,
  // evenement action
  doReset,
};
const uint32_t checkWWW_DELAY = (60 * 60 * 1000L);
const uint32_t  checkAPI_DELAY  = (2 * 60 * 1000L);
const uint32_t  DS18X_DELAY  = (5 * 60 * 1000L);  // lecture des sondes toute les 5 minutes

// instance betaEvent

//  une instance "MyEvents" avec un poussoir "MyBP0" une LED "Led0" un clavier "Keyboard"
//  MyBP0 genere un evenement evBP0 a chaque pression le poussoir connecté sur D2
//  Led0 genere un evenement evLed0 a chaque clignotement de la led precablée sur la platine
//  Keyboard genere un evenement evChar a char caractere recu et un evenement evString a chaque ligne recue
//  MyDebug permet sur reception d'un "T" sur l'entrée Serial d'afficher les infos de charge du CPU
#define DEBUG_ON
#define BP0_PIN  D2  // flash button
#define LED0_PIN LED_BUILTIN // D16
#include <BetaEvents.h>
#define ONEWIRE_PIN D4
#define BEEP_PIN D5
#define NOT_A_DATE_YEAR   2000

// Sondes temperatures : DS18B20
//instance du bus OneWire dedié aux DS18B20
#include "evHandlerDS18x20.h"

evHandlerDS18x20 ds18x(ONEWIRE_PIN, DS18X_DELAY);



// littleFS
#include <LittleFS.h>  //Include File System Headers 
#define MyLittleFS  LittleFS

//WiFI
#include <ESP8266WiFi.h>
//#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>
//WiFiClientSecure client;

// rtc memory to keep date
//struct __attribute__((packed))
struct  {
  // all these values are keep in RTC RAM
  uint8_t   crc8;                 // CRC for savedRTCmemory
  time_t    actualTimestamp;      // time stamp restored on next boot Should be update in the loop() with setActualTimestamp
} savedRTCmemory;


// Variable d'application locale
String   nodeName = "NODE_NAME";    // nom de  la device (a configurer avec NODE=)"

bool     WiFiConnected = false;
time_t   currentTime;
bool     configErr = false;
bool     WWWOk = false;
bool     APIOk = false;
int      currentMonth = -1;
bool sleepOk = true;
int  multi = 0; // nombre de clic rapide
bool     configOk = true; // global used by getConfig...
String  mailSendTo;       // mail to send email
int8_t   timeZone = 0; //-2;  //les heures sont toutes en localtimes (par defaut hivers france)
int8_t   sondesNumber = 0;  // nombre de sonde
String  sondesName[MAXDS18x20];  // noms des sondes
float   sondesValue[MAXDS18x20];  // valeur des sondes


// init UDP
#include  "evHandlerUdp.h"
const unsigned int localUdpPort = 23423;      // local port to listen on
evHandlerUdp myUdp(evUdp, localUdpPort, nodeName);


void setup() {
  enableWiFiAtBootTime();  // mendatory for autoconnect WiFi with ESP8266 kernel 3.0
  Serial.begin(115200);
  Serial.println(F("\r\n\n" APP_NAME));
  D_println(sizeof(stdEvent_t));
  delay(3000);
  // Start instance
  Events.begin();

  D_println(WiFi.getMode());

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
  D_println(nodeName);

  // recuperation de la timezone dans la config
  timeZone = jobGetConfigInt(F("timezone"));
  if (!configOk) {
    timeZone = -2; // par defaut France hivers
    jobSetConfigInt(F("timezone"), timeZone);
    Serial.println(F("!!! timezone !!!"));
  }
  D_println(timeZone);

  // recuperation des donnée pour les mails dans la config
  String aSmtpServer = jobGetConfigStr(F("smtpserver"));
  if (aSmtpServer == "") {
    Serial.println(F("!!! Configurer le serveur smtp 'SMTPSERV=smtp.monserveur.mail' !!!"));
    configErr = true;
  }
  D_println(aSmtpServer);

  mailSendTo = jobGetConfigStr(F("mailto"));
  if (mailSendTo == "") {
    Serial.println(F("!!! Configurer l'adresse pour le mail  'MAILTO=monAdresseMail' !!!"));
    configErr = true;
  }
  D_println(mailSendTo);


  String currentMessage;
  if (configErr) {
    currentMessage = F("Config Error");
    beep( 880, 500);
    delay(500);
  } else {
    currentMessage = F("device=");
    currentMessage += nodeName;
    // a beep
    beep( 880, 500);
    delay(500);
    beep( 988, 500);
    delay(500);
    beep( 1047, 500);
  }
  D_println(currentMessage);
  D_println(Events.freeRam());




  // Recuperation du nom des sondes
  sondesNumber = ds18x.getNumberOfDevices();
  if (sondesNumber > MAXDS18x20) sondesNumber = MAXDS18x20;
  D_println(sondesNumber);
  jobGetSondeName();

  // start OTA
  String deviceName = nodeName; // "ESP_";

  ArduinoOTA.setHostname(deviceName.c_str());
  ArduinoOTA.begin();
  //MDNS.update();
  Serial.print("OTA on '");
  Serial.print(deviceName);
  Serial.println("' started.");
  Serial.print("SSID:");
  Serial.println(WiFi.SSID());
  //end start OTA


  Serial.println("Bonjour ....");
  Serial.println("Tapez '?' pour avoir la liste des commandes");
  D_println(LED_BUILTIN);

}

byte BP0Multi = 0;

String niceDisplayTime(const time_t time, bool full = false);

void loop() {
  ArduinoOTA.handle();
  Events.get(sleepOk);
  Events.handle();
  switch (Events.code)
  {
    case evInit:
      Serial.println("Init");
      writeHisto( F("Boot"), nodeName );
      Events.delayedPush(1000L * 15 * 60, evStopOta); // stop OTA dans 5 Min
      myUdp.broadcast("{\"info\":\"Boot\"}");
      break;


    case evStopOta:
      Serial.println("Stop OTA");
      myUdp.broadcast("{\"info\":\"stop OTA\"}");
      ArduinoOTA.end();
      writeHisto( F("Stop OTA"), nodeName );
      break;

    case ev24H: {
        String newDateTime = niceDisplayTime(currentTime, true);
        D_println(newDateTime);
        writeHisto( F("newDateTime"), newDateTime );
      }
      break;

    case ev1Hz: {
        // check for connection to local WiFi  1 fois par seconde c'est suffisant
        static uint8_t oldWiFiStatus = 99;
        uint8_t  WiFiStatus = WiFi.status();
        if (oldWiFiStatus != WiFiStatus) {
          oldWiFiStatus = WiFiStatus;
          D_println(WiFiStatus);
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
          static bool wasConnected = WiFiConnected;
          if (wasConnected != WiFiConnected) {
            wasConnected = WiFiConnected;
            Led0.setFrequence(WiFiConnected ? 1 : 2);
            if (WiFiConnected) {
              setSyncProvider(getWebTime);
              setSyncInterval(6 * 3600);
              // lisen UDP 23423
              Serial.println("Listen broadcast");
              myUdp.begin();
              Events.delayedPush(checkWWW_DELAY, evCheckWWW); // will send mail
              Events.delayedPush(checkAPI_DELAY, evCheckAPI);
            } else {
              WWWOk = false;
            }
            D_println(WiFiConnected);
            writeHisto( WiFiConnected ? F("wifi Connected") : F("wifi lost"), WiFi.SSID() );
          }
        }

        // save current time in RTC memory
        currentTime = now();
        savedRTCmemory.actualTimestamp = currentTime;  // save time in RTC memory
        saveRTCmemory();

        // If we are not connected we warn the user every 30 seconds that we need to update credential


        if ( !WiFiConnected && second() % 30 ==  15) {
          // every 30 sec
          Serial.print(F("module non connecté au Wifi local "));
          D_println(WiFi.SSID());
          Serial.println(F("taper WIFI= pour configurer le Wifi"));
        }

        // au chagement de mois a partir 7H25 on envois le mail (un essais par heure)
        if (WiFiConnected && currentMonth != month() && hour() > 7 && minute() == 25 && second() == 0) {
          if (sendHistoTo(mailSendTo)) {
            if (currentMonth > 0) eraseHisto();
            currentMonth = month();
            writeHisto( F("Mail send ok"), mailSendTo );
          } else {
            writeHisto( F("Mail erreur"), mailSendTo );
          }
        }
      }
      break;

    case evCheckWWW:
      Serial.println("evCheckWWW");
      if (WiFiConnected) {
        if (WWWOk != (getWebTime() > 0)) {
          WWWOk = !WWWOk;
          D_println(WWWOk);
          writeHisto( WWWOk ? F("WWW Ok") : F("WWW Err"), "www.free.fr" );
          if (WWWOk) {
            Serial.println("send a mail");
            bool sendOk = sendHistoTo(mailSendTo);
            if (sendOk) {
              //eraseHisto();
              writeHisto( F("Mail send ok"), mailSendTo );
            } else {
              writeHisto( F("Mail erreur"), mailSendTo );
            }
          }
        }
        Events.delayedPush(checkWWW_DELAY, evCheckWWW);
      }
      break;

    case evCheckAPI: {
        Serial.println("evCheckAPI");
        if (WiFiConnected) {
          JSONVar jsonData;
          jsonData["timeZone"] = timeZone;
          jsonData["timestamp"] = (double)currentTime;
          for (  int N = 0; N < sondesNumber; N++) {
            //D_println(sondesName[N]);
            //D_println(sondesValue[N]);
            jsonData[sondesName[N]] = sondesValue[N];
          }
          String jsonStr = JSON.stringify(jsonData);
          if ( APIOk != dialWithPHP(nodeName, "timezone", jsonStr) ) {
            APIOk = !APIOk;
            D_println(APIOk);
            writeHisto( APIOk ? F("API Ok") : F("API Err"), "magnus2.frdev" );
          }
          if (APIOk) {
            jsonData = JSON.parse(jsonStr);
            time_t aTimeZone = (const double)jsonData["timezone"];
            D_println(aTimeZone);
            if (aTimeZone != timeZone) {
              writeHisto( F("Old TimeZone"), String(timeZone) );
              timeZone = aTimeZone;
              jobSetConfigInt("timezone", timeZone);
              // force recalculation of time
              setSyncProvider(getWebTime);
              currentTime = now();
              writeHisto( F("New TimeZone"), String(timeZone) );
            }
          }
        }
        Events.delayedPush(checkAPI_DELAY, evCheckAPI);
      }
      break;

    // lecture des sondes
    case evSonde1 ... evSondeMAX: {
        int aSonde = Events.code - evSonde1;
        //D_println(aSonde + 1);
        sondesValue[aSonde] = Events.intExt / 100.0;

        Serial.print(sondesName[aSonde]);
        Serial.print(" : ");
        Serial.println(sondesValue[aSonde]);
        String aTxt = "{\"temperature\":{\"";
        aTxt += sondesName[aSonde];
        aTxt += "\":";
        aTxt += sondesValue[aSonde];
        aTxt += "}}";

        // if (WiFiConnected) {
        TD_println("BroadCast", aTxt);
        myUdp.broadcast(aTxt);
        //      }

      }

      break;

    // lecture des sondes


    case evDs18x20: {
        if (Events.ext == evxDsRead) {
          //          if (ds18x.error) {
          //            D_println(ds18x.error);
          //            // TODO: gerer le moErreur;
          //            break;
          //          };
          //          D_println(ds18x.current);
          //          D_println(ds18x.celsius());
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
          Led0.setMillisec(500, 50);
          BP0Multi++;
          Serial.println(F("BP0 Down"));
          if (BP0Multi > 1) {
            Serial.print(F("BP0 Multi ="));
            Serial.println(BP0Multi);
          }
          break;
        case evxOff:
          Led0.setMillisec(1000, 10);
          Serial.println(F("BP0 Up"));
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

    case evUdp:
      if (Events.ext == evxUdpRxMessage) {
        TD1_println("got an Event UDP", myUdp.rxJson);
        String aStr = grabFromStringUntil(myUdp.rxJson,F("{\"CMD\":{\""));
        if (myUdp.rxJson.length() == 0) {
          TD_println("Not a CMD",aStr);
          break;
        }

        aStr = grabFromStringUntil(myUdp.rxJson,'"');
        if ( not aStr.equals(nodeName)) {
          TD_println("CMD not for me",aStr);
          break;
        }
        grabFromStringUntil(myUdp.rxJson,'"');
        aStr = grabFromStringUntil(myUdp.rxJson,'"');
        aStr.trim();
        if (aStr.length()) Keyboard.setInputString(aStr);
      }
      break;

    case doReset:
      Events.reset();
      break;


    //    case evInChar: {
    //        if (MyDebug.trackTime < 2) {
    //          char aChar = Keyboard.inputChar;
    //          if (isPrintable(aChar)) {
    //            D_println(aChar);
    //          } else {
    //            D_println(int(aChar));
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
      D_println(Keyboard.inputString);
      if (Keyboard.inputString.startsWith(F("?"))) {
        Serial.println(F("Liste des commandes"));
        Serial.println(F("NODE=nodename (nom du module)"));
        Serial.println(F("WIFI=ssid,paswword"));
        Serial.println(F("MAILTO=adresse@mail    (mail du destinataire)"));
        Serial.println(F("MAILFROM=adresse@mail  (mail emetteur 'NODE' sera remplacé par nodename)"));
        Serial.println(F("SMTPSERV=mail.mon.fai,login,password  (SMTP serveur et credential) "));
        Serial.println(F("SONDENAMES=name1,name2...."));
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
          D_println(nodeName);
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
        D_println(ssid);
        if (ssid != "") {
          String pass = aStr;
          pass.trim();
          D_println(pass);
          bool result = WiFi.begin(ssid, pass);
          //WiFi.setAutoConnect(true);
          D_println(WiFi.getAutoConnect());
          Serial.print(F("WiFi begin "));
          D_println(result);
        }

      }
      if (Keyboard.inputString.startsWith(F("MAILTO="))) {
        Serial.println(F("SETUP mail to  : 'MAILTO=monAdresseMail@monfai'"));
        String aMail = Keyboard.inputString;
        grabFromStringUntil(aMail, '=');
        aMail.trim();
        D_println(aMail);
        if (aMail != "") {
          jobSetConfigStr(F("mailto"), aMail);
          mailSendTo = aMail;
        }
      }


      if (Keyboard.inputString.startsWith(F("MAILFROM="))) {
        Serial.println(F("SETUP mail to  : 'MAILFROM=NODE@monfai'"));
        String aMail = Keyboard.inputString;
        grabFromStringUntil(aMail, '=');
        aMail.trim();
        D_println(aMail);
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
        D_println(aSmtp);
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
        D_println(Events.freeRam());
      }
      if (Keyboard.inputString.equals(F("HIST"))) {
        printHisto();
      }
      if (Keyboard.inputString.equals(F("CONF"))) {
        jobShowConfig();
      }

      if (Keyboard.inputString.equals(F("MAIL"))) {
        bool mailHisto = sendHistoTo(mailSendTo);
        D_println(mailHisto);
      }

      if (Keyboard.inputString.equals("S")) {
        sleepOk = !sleepOk;
        D_println(sleepOk);
      }
      if (Keyboard.inputString.equals("API")) {
        String jsonData = "";
        bool dialWPHP = dialWithPHP(nodeName, "timezone", jsonData);
        D_println(jsonData);
        D_println(dialWPHP);
      }


      if (Keyboard.inputString.equals("BCAST")) {
        myUdp.broadcast("{\"info\":\"test broadcast\"}");
        T_println("test broadcast");
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
