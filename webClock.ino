// get the time with a standard web server
#include <ESP8266HTTPClient.h>
//time_t webClockLastTry;
//int16_t webClockDelta;

time_t getWebTime() {
  // connect to a captive portal to get time (theses portal are used by any navigators to check web connections)
  //  safari     captive.apple.com  timestamp around 4 second false (use cache to fast redirect)
  //  chrome     connectivitycheck.gstatic.com
  //  firefox    detectportal.firefox.com
  //  edge       www.msftncsi.com  timestamp about 1 second
  //  https://success.tanaza.com/s/article/How-Automatic-Detection-of-Captive-Portal-works
  // in fact any descent http web server redirect on https  so use your FAI web url (dont call it every seconds :)
  // mine is www.free.fr

//#define  HTTP_SERVER  "www.free.fr"  // my FAI web server

  //Serial.println(F("connect to " HTTP_SERVER " to get time"));
  if (!WiFiConnected) {
    DT_println("!! NO WIFI to syncr clock !!");
    return (0);
  }

  String urlServer = "http://";
  urlServer += WiFi.gatewayIP().toString();

  DTV_println("Gateway",urlServer);


  WiFiClient client;  // Wificlient doit etre declaré avant HTTPClient
  HTTPClient http;  //Declare an object of class HTTPClient (Gsheet and webclock)


  //  HTTPClient http;  //Declare an object of class HTTPClient

  http.begin(client, urlServer); //Specify request destination
  // we need date to setup clock so
  const char * headerKeys[] = {"date"} ;
  const size_t numberOfHeaders = 1;
  http.collectHeaders(headerKeys, numberOfHeaders);

  int httpCode = http.GET();                                  //Send the request
  if (httpCode < 0) {
    Serial.print(F("cant get an answer :( http.GET()="));
    Serial.println(httpCode);
    http.end();   //Close connection
    return (0);
  }

  // we got an answer the date is in the header so we grab it
  tmElements_t dateStruct;
  {
    String headerDate = http.header(headerKeys[0]);

    // Check the header should be a 29 char texte like this 'Mon, 24 May 2021 13:57:04 GMT'
    //D_println(headerDate);
    if (!headerDate.endsWith(" GMT") || headerDate.length() != 29) {
      Serial.println(F("reponse invalide :("));
      http.end();   //Close connection
      return 0;
    }

    //time_t makeTime(const tmElements_t &tm);  // convert time elements into time_t
    //typedef struct  {
    //  uint8_t Second;
    //  uint8_t Minute;
    //  uint8_t Hour;
    //  uint8_t Wday;   // day of week, sunday is day 1
    //  uint8_t Day;
    //  uint8_t Month;
    //  uint8_t Year;   // offset from 1970;
    //}   tmElements_t, TimeElements, *tmElementsPtr_t;


    // Grab date from the header
    dateStruct.Second  = headerDate.substring(23, 25).toInt();
    dateStruct.Minute  = headerDate.substring(20, 22).toInt();
    dateStruct.Hour = headerDate.substring(17, 19).toInt();
    dateStruct.Year = headerDate.substring(12, 16).toInt() - 1970;
    const String monthName = F("JanFebMarAprMayJunJulAugSepOctNovDec");
    dateStruct.Month = monthName.indexOf(headerDate.substring(8, 11)) / 3 + 1;
    dateStruct.Day = headerDate.substring(5, 7).toInt();
  }
  http.end();   //Close connection

  time_t serverTS = makeTime(dateStruct) - (timeZone * 3600); // change to local time
  webClockDelta = serverTS - currentTime;
  DV_println(webClockDelta);
  webClockLastTry = serverTS;
  return serverTS;
}
