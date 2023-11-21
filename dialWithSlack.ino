
//#include <ESP8266WiFi.h>
//#include <WiFiClientSecure.h>
// acessing a webhook slack
// POST -H 'Content-type: application/json' --data '{"text":"Hello, World!"}'

#define HOOK_SERVER "hooks.slack.com/services/"
// need about 30K of ram !!!!!! WiFiClientSecure
// todo return better errcode
bool dialWithSlack(const String& aMsg) {
  String skey=jobGetConfigStr(F("skey"));
  if (skey.length() ==0) {
    T_println("No Skey");
    return(false);
  }
  TD_println("Start freeram", Events.freeRam());
  bool result = true;
  {
    WiFiClientSecure client;  // 7K
    HTTPClient http;          //Declare an object of class
    http.setTimeout(5000);    // 5 Seconds   (could be long with google)
    TD_println("HTTPClient freeram", Events.freeRam());
    T_println(F("Dial With slack"));

    if (Events.freeRam() < 35000) {
      T_println("https need more memory");
      return (false);
    }


    // Construire l'URL pour la requête POST
    String url = F("https://" HOOK_SERVER);
    url += skey;
    D_println(url);
    client.setInsecure();  //the magic line, use with caution  !!! certificate not checked
    // Commencer la requête POST
    http.begin(client, url);
    // Configurer le type de contenu et envoyer la requête POST
    http.addHeader("Content-type", "application/json");
    String aJson = F("{\"text\":\"");
    aJson += aMsg;
    aJson += F("\"}");
    int httpResponseCode = http.POST(aJson);

    D_println(httpResponseCode);
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
  TD_println("End freeram", Events.freeRam());
  return (result);
}
