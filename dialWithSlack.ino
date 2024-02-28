
//#include <ESP8266WiFi.h>
//#include <WiFiClientSecure.h>
// acessing a webhook slack
// POST -H 'Content-type: application/json' --data '{"text":"Hello, World!"}'

#define HOOK_SERVER "hooks.slack.com/services/"
// need about 30K of ram !!!!!! WiFiClientSecure
// todo return better errcode

bool dialWithSlack(const String& aMsg) {
  //  String skey=jobGetConfigStr(F("skey"));
  //  if (skey.length() ==0) {
  //    T_println("No Skey");
  //    return(false);
  //  }
  DTV_println("Start freeram", Events.freeRam());
  bool result = true;
  {
    WiFiClientSecure client;  // 7K
    HTTPClient http;          //Declare an object of class
    http.setTimeout(5000);    // 5 Seconds   (could be long with google)
    DTV_println("HTTPClient freeram", Events.freeRam());
    T_println(F("Dial With slack"));

    if (Events.freeRam() < 35000) {
      T_println("https need more memory");
      return (false);
    }


    // Construire l'URL pour la requête POST
    String url = F("https://" HOOK_SERVER);
    url += skey;
    DV_println(url);
    client.setInsecure();  //the magic line, use with caution  !!! certificate not checked
    // Commencer la requête POST
    http.begin(client, url);
    // Configurer le type de contenu et envoyer la requête POST
    http.addHeader("Content-type", "application/json");
    String aJson = F("{\"text\":\"");
    aJson += aMsg;
    aJson += F("\"}");
    int httpResponseCode = http.POST(aJson);

    DV_println(httpResponseCode);
    if (httpResponseCode > 0) {
      Serial.print("Réponse du serveur : ");
      Serial.println(http.getString());
    } else {
      Serial.print("Erreur lors de la requête : ");
      Serial.println(http.errorToString(httpResponseCode).c_str());
      Serial.println(http.getString());
      result = false;
    }

    http.end();
  }
  DTV_println("End freeram", Events.freeRam());
  return (result);
}


bool dialWithSonoffHall() {
  //  String skey=jobGetConfigStr(F("skey"));
  //  if (skey.length() ==0) {
  //    T_println("No Skey");
  //    return(false);
  //  }
  DTV_println("dialWithSonoffHall freeram", Events.freeRam());
  bool result = true;
  {
    WiFiClient client;  // 7K
    HTTPClient http;          //Declare an object of class
    http.setTimeout(5000);    // 5 Seconds   (could be long with mDns)
    DTV_println("HTTPClient freeram", Events.freeRam());
    T_println(F("Dial With sonoff"));

    if (Events.freeRam() < 35000) {
      T_println("http need more memory");
      return (false);
    }


    // Construire l'URL pour la requête POST
    String url = F("http://");
    url += hallkey;
    DV_println(url);
    //client.setInsecure();  //the magic line, use with caution  !!! certificate not checked
    // Commencer la requête GET
    http.begin(client, url);

   // Configurer le type de contenu et envoyer la requête GET
      int httpResponseCode = http.GET();

    DV_println(httpResponseCode);
    if (httpResponseCode > 0) {
      DTV_println("Réponse du serveur : ",httpResponseCode);
      //Serial.println(http.getString());
    } else {
      Serial.print("Erreur lors de la requête : ");
      Serial.println(http.errorToString(httpResponseCode).c_str());
      //Serial.println(http.getString());
      result = false;
    }

    http.end();
  }
  DTV_println("End freeram", Events.freeRam());
  return (result);

}
