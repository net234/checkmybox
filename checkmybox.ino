/*************************************************
 *************************************************
    Sketch checkMyBox.ino   check and report by may a box and the wifi connectivity
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


 *************************************************/

#define APP_NAME "checkMyBox V1.0"
#include "private.h"
// private should include
//#define  PRIVATE_SEND_TO       mail receptor : "monmail@mondomaine.fr"
//#define  PRIVATE_SEND_FROM     mail sender : "nodename@mondomaine.fr"
//#define  PRIVATE_SMTP_SERVER   mail server  "smtp.monOperateur.fr"
//#define  PRIVATE_SMTP_LOGIN    base64 encoded login    "fZQ34...        ...RtYqz"
//#define  PRIVATE_SMTP_PASS     base64 encoded password "gT54z...        ...azE=="

/* Evenements du Manager (voir EventsManager.h)
  evNill = 0,      // No event  about 1 every milisecond but do not use them for delay Use pushDelayEvent(delay,event)
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
  evCheckWWW,
  evCheckAPI,
  // evenement action
  doReset,
};

#define checkWWW_DELAY  (60 * 60 * 1000)
#define checkAPI_DELAY   (5 * 60 * 1000)

// instance betaEvent

//  une instance "MyEvents" avec un poussoir "MyBP0" une LED "MyLed0" un clavier "MyKeyboard"
//  MyBP0 genere un evenement evBP0 a chaque pression le poussoir connecté sur D2
//  MyLed0 genere un evenement evLed0 a chaque clignotement de la led precablée sur la platine
//  MyKeyboard genere un evenement evChar a char caractere recu et un evenement evString a chaque ligne recue
//  MyDebug permet sur reception d'un "T" sur l'entrée Serial d'afficher les infos de charge du CPU

#define pinBP0  D5                 //   By default BP0 is on D2 you can change it
//#define pinLed0  3 //LED_BUILTIN   //   By default Led0 is on LED_BUILTIN you can change it
#include <BetaEvents.h>
#define BEEP_PIN D3
#define NOT_A_DATE_YEAR   2000

// littleFS
#include <LittleFS.h>  //Include File System Headers 
#define MyLittleFS  LittleFS

//WiFI
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>
//WiFiClientSecure client;
HTTPClient http;  //Declare an object of class HTTPClient (Gsheet and webclock)


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


void setup() {

  Serial.begin(115200);
  Serial.println(F("\r\n\n" APP_NAME));

  // Start instance
  MyEvents.begin();

  D_println(WiFi.getMode());
  // normaly not needed
  if (WiFi.getMode() != WIFI_STA) {
    Serial.println(F("!!! Force WiFi to STA mode !!!"));
    WiFi.mode(WIFI_STA);
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
  }
  D_println(nodeName);

  // recuperation de la timezone dans la config
  timeZone = jobGetConfigInt(F("timezone"));
  //  if (configOk) {
  //    timeZone = aTimeZone;
  //    if (timeZone < -12 || timeZone > 12) timeZone = -2;
  //  } else {
  //    jobSetConfigInt(F("timezone"),timeZone);
  //    Serial.println(F("!!! timezone !!!"));
  //  }
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
  D_println(MyEvents.freeRam());


  Serial.println("Bonjour ....");
  //D_println(LED_BUILTIN);

}

byte BP0Multi = 0;


void loop() {

  MyEvents.getEvent(sleepOk);
  MyEvents.handleEvent();
  switch (MyEvents.currentEvent.code)
  {
    case evInit:
      Serial.println("Init");
      writeHisto( F("Boot"), nodeName );
      break;


    case ev24H:
      Serial.println("---- 24H ---");
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
          WiFiConnected = (WiFiStatus == WL_CONNECTED);
          static bool wasConnected = WiFiConnected;
          if (wasConnected != WiFiConnected) {
            wasConnected = WiFiConnected;
            MyLed0.setFrequence(WiFiConnected ? 1 : 2);
            if (WiFiConnected) {
              setSyncProvider(getWebTime);
              setSyncInterval(6 * 3600);
              MyEvents.pushDelayEvent(checkWWW_DELAY, evCheckWWW); // will send mail
              MyEvents.pushDelayEvent(checkAPI_DELAY, evCheckAPI);
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
        MyEvents.pushDelayEvent(checkWWW_DELAY, evCheckWWW);
      }
      break;

    case evCheckAPI: {
        Serial.println("evCheckAPI");
        if (WiFiConnected) {
          JSONVar jsonData;
          jsonData["timeZone"] = timeZone;
          jsonData["timestamp"] = currentTime;
          if ( APIOk != dialWithPHP(nodeName, "timezone", jsonData) ) {
            APIOk = !APIOk;
            D_println(APIOk);
            writeHisto( APIOk ? F("API Ok") : F("API Err"), "magnus2.frdev" );
          }
          if (APIOk) {
            time_t aTimeZone = (time_t)jsonData["timezone"];
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
        MyEvents.pushDelayEvent(checkAPI_DELAY, evCheckAPI);
      }
      break;


    case evBP0:
      switch (MyEvents.currentEvent.ext) {
        case evxBPDown:
          MyLed0.setMillisec(500, 50);
          BP0Multi++;
          Serial.println(F("BP0 Down"));
          if (BP0Multi > 1) {
            Serial.print(F("BP0 Multi ="));
            Serial.println(BP0Multi);
          }
          break;
        case evxBPUp:
          MyLed0.setMillisec(1000, 10);
          Serial.println(F("BP0 Up"));
          break;
        case evxBPLongDown:
          if (BP0Multi == 5) {
            Serial.println(F("RESET"));
            MyEvents.pushEvent(doReset);
          }

          Serial.println(F("BP0 Long Down"));
          break;
        case evxBPLongUp:
          BP0Multi = 0;
          Serial.println(F("BP0 Long Up"));
          break;

      }
      break;

    case doReset:
      helperReset();
      break;


    case evInChar: {
        if (MyDebug.trackTime < 2) {
          char aChar = MyKeyboard.inputChar;
          if (isPrintable(aChar)) {
            D_println(aChar);
          } else {
            D_println(int(aChar));
          }
        }
        switch (toupper(MyKeyboard.inputChar))
        {
          case '0': delay(10); break;
          case '1': delay(100); break;
          case '2': delay(200); break;
          case '3': delay(300); break;
          case '4': delay(400); break;
          case '5': delay(500); break;

        }
      }
      break;



    case evInString:
      if (MyDebug.trackTime < 2) {
        D_println(MyKeyboard.inputString);
      }

      if (MyKeyboard.inputString.startsWith(F("NODE="))) {
        Serial.println(F("SETUP NODENAME : 'NODE=nodename'  ( this will reset)"));
        String aStr = MyKeyboard.inputString;
        grabFromStringUntil(aStr, '=');
        aStr.replace(" ", "_");
        aStr.trim();

        if (aStr != "") {
          nodeName = aStr;
          D_println(nodeName);
          jobSetConfigStr(F("nodename"), nodeName);
          delay(1000);
          helperReset();
        }
      }


      if (MyKeyboard.inputString.startsWith(F("WIFI="))) {
        Serial.println(F("SETUP WIFI : 'WIFI=WifiName,password"));
        String aStr = MyKeyboard.inputString;
        grabFromStringUntil(aStr, '=');
        String ssid = grabFromStringUntil(aStr, ',');
        ssid.trim();
        D_println(ssid);
        if (ssid != "") {
          String pass = aStr;
          pass.trim();
          D_println(pass);
          bool result = WiFi.begin(ssid, pass);
          Serial.print(F("WiFi begin "));
          D_println(result);
        }

      }
      if (MyKeyboard.inputString.startsWith(F("MAILTO="))) {
        Serial.println(F("SETUP mail to  : 'MAILTO=monAdresseMail@monfai'"));
        String aMail = MyKeyboard.inputString;
        grabFromStringUntil(aMail, '=');
        aMail.trim();
        D_println(aMail);
        if (aMail != "") jobSetConfigStr(F("mailto"), aMail);
      }


      if (MyKeyboard.inputString.startsWith(F("MAILFROM="))) {
        Serial.println(F("SETUP mail to  : 'MAILFROM=NODE@monfai'"));
        String aMail = MyKeyboard.inputString;
        grabFromStringUntil(aMail, '=');
        aMail.trim();
        D_println(aMail);
        if (aMail != "") jobSetConfigStr(F("mailfrom"), aMail);
      }



      

      if (MyKeyboard.inputString.startsWith(F("SMTPSERV="))) {
        Serial.println(F("SETUP smtp serveur : 'SMTPSERV=smtp.mon_serveur.xx,login,pass'"));
        String aStr = MyKeyboard.inputString;
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




      if (MyKeyboard.inputString.equals(F("RAZCONF"))) {
        Serial.println(F("RAZCONF this will reset"));
        eraseConfig();
        delay(1000);
        helperReset();
      }



      if (MyKeyboard.inputString.equals(F("RESET"))) {
        Serial.println(F("RESET"));
        MyEvents.pushEvent(doReset);
      }
      if (MyKeyboard.inputString.equals(F("FREE"))) {
        D_println(MyEvents.freeRam());
      }
      if (MyKeyboard.inputString.equals(F("HIST"))) {
        printHisto();
      }
      if (MyKeyboard.inputString.equals(F("CONF"))) {
        jobShowConfig();
      }

      if (MyKeyboard.inputString.equals(F("MAIL"))) {
        bool mailHisto = sendHistoTo(mailSendTo);
        D_println(mailHisto);
      }

      if (MyKeyboard.inputString.equals("S")) {
        sleepOk = !sleepOk;
        D_println(sleepOk);
      }
      if (MyKeyboard.inputString.equals("PHP")) {
        JSONVar jsonData;
        bool dialWPHP = dialWithPHP(nodeName, "timezone", jsonData);
        D_println(JSON.stringify(jsonData));
        D_println(dialWPHP);
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
      MyLed0.setOn(true);
      beep(988, 100);
      delay(150);
      MyLed0.setOn(false);
    }
    delay(500);
  }
  delay(2000);
  helperReset();

}


void beep(const uint16_t frequence, const uint16_t duree) {
  tone(BEEP_PIN, frequence, duree);
}


String grabFromStringUntil(String &aString, const char aKey) {
  String result = "";
  int pos = aString.indexOf(aKey);
  if ( pos == -1 ) {
    result = aString;
    aString = "";
    return (result);  // not match
  }
  result = aString.substring(0, pos);
  //aString = aString.substring(pos + aKey.length());
  aString = aString.substring(pos + 1);
  D_println(result);
  D_println(aString);
}
