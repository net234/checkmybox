// get the time with a standard web server
#include <ESP8266HTTPClient.h>
#define LOCATION "Europe/Paris"  // adjust to yout location
#define URI "worldtimeapi.org/api/timezone/"

time_t getWWWTime() {
  // connect to an internet API to
  // check WWW connection
  // get unix time
  // get timezone
  //http://worldtimeapi.org/api/timezone/Europe/Paris

  String urlServer = F("http://" URI LOCATION);
  DV_println(urlServer);
  WiFiClient aWiFI;  // Wificlient doit etre declaré avant HTTPClient
  HTTPClient aHttp;  //Declare an object of class HTTPClient
  aHttp.setTimeout(100);
  aHttp.begin(aWiFI, urlServer);  //Specify request destination

  int httpCode = aHttp.GET();  //Send the request
  if (httpCode != 200) {
    DTV_println(F("got an error in http.GET() "), httpCode);
    aHttp.end();  //Close connection
    return (0);
  }

  String answer = aHttp.getString();  //Get the request response payload
  //DV_print(helperFreeRam() + 1);
  aHttp.end();         //Close connection (restore 22K of ram)
  DV_println(answer);  //Print the response payload
                       //  23:02:48.615 -> answer => '{"abbreviation":"CET","client_ip":"82.66.229.100","datetime":"2024-03-15T23:02:48.596866+01:00","day_of_week":5,"day_of_year":75,"dst":false,"dst_from":null,"dst_offset":0,"dst_until":null,"raw_offset":3600,"timezone":"Europe/Paris","unixtime":1710540168,"utc_datetime":"2024-03-15T22:02:48.596866+00:00","utc_offset":"+01:00","week_number":11}'
  if (!answer.startsWith(F("{\"abbreviation\":"))) return (0);
  DV_println(helperFreeRam());
  JSONVar aJson = JSON.parse(answer);
  DV_println(helperFreeRam());
  int timezone = (const double)aJson["raw_offset"];
  time_t unixtime = (const long)aJson["unixtime"] + timezone;
  timezone = timezone / -3600;
  DV_println(unixtime);
  DV_println(timezone);
 webClockDelta = unixtime - bNode.currentTime;
  DV_println(webClockDelta);
  webClockLastTry = unixtime;
  return (unixtime);
}




// get the time with a standard web server
//#include <ESP8266HTTPClient.h>
//time_t webClockLastTry;
//int16_t webClockDelta;

time_t getGatewayTime() {
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
  if (!Wifi.connected) {
    DT_println("!! NO WIFI to syncr clock !!");
    return (0);
  }

  String urlServer = "http://";
  urlServer += WiFi.gatewayIP().toString();


  DTV_println("Gateway", urlServer);


  WiFiClient client;  // Wificlient doit etre declaré avant HTTPClient
  HTTPClient http;    //Declare an object of class HTTPClient (Gsheet and webclock)


  //  HTTPClient http;  //Declare an object of class HTTPClient

  http.begin(client, urlServer);  //Specify request destination
  // we need date to setup clock so
  const char* headerKeys[] = { "date" };
  const size_t numberOfHeaders = 1;
  http.collectHeaders(headerKeys, numberOfHeaders);

  int httpCode = http.GET();  //Send the request
  if (httpCode < 0) {
    Serial.print(F("cant get an answer :( http.GET()="));
    Serial.println(httpCode);
    http.end();  //Close connection
    return (0);
  }

  // we got an answer the date is in the header so we grab it
  tmElements_t dateStruct;
  {
    String headerDate = http.header(headerKeys[0]);

    // Check the header should be a 29 char texte like this 'Mon, 24 May 2021 13:57:04 GMT'
    //DV_print(headerDate);
    if (!headerDate.endsWith(" GMT") || headerDate.length() != 29) {
      Serial.println(F("reponse invalide :("));
      http.end();  //Close connection
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
    dateStruct.Second = headerDate.substring(23, 25).toInt();
    dateStruct.Minute = headerDate.substring(20, 22).toInt();
    dateStruct.Hour = headerDate.substring(17, 19).toInt();
    dateStruct.Year = headerDate.substring(12, 16).toInt() - 1970;
    const String monthName = F("JanFebMarAprMayJunJulAugSepOctNovDec");
    dateStruct.Month = monthName.indexOf(headerDate.substring(8, 11)) / 3 + 1;
    dateStruct.Day = headerDate.substring(5, 7).toInt();
  }
  http.end();  //Close connection

  time_t serverTS = makeTime(dateStruct) - (bNode.timeZone * 3600);  // change to local time
  webClockDelta = serverTS - bNode.currentTime;
  DV_println(webClockDelta);
  webClockLastTry = serverTS;
  return serverTS;
}
