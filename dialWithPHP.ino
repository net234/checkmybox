// make an http get on a specific server and get json answer
#include <ESP8266HTTPClient.h>
// passage du json en string
//

bool dialWithPHP(const String& aNode, const String& aAction,  String& jsonParam) {
  //D_println(helperFreeRam() + 000);
  Serial.print(F("Dial With http as '"));
  Serial.print(aNode);
  Serial.print(':');
  Serial.print(aAction);
  Serial.println('\'');
  //{
  String aUri =  "http://nimux.frdev.com/net234/api.php?action=";  // my FAI web server
  aUri += encodeUri(aAction);
  aUri += F("&node=");
  aUri += encodeUri(aNode);;

  // les parametres eventuels sont passées en JSON dans le parametre '&json='
  if (jsonParam.length() > 0) {
    aUri += F("&json=");
    //D_println(JSON.stringify(jsonParam));
    aUri += encodeUri(jsonParam);
  }
  //D_println(helperFreeRam() + 0001);
  jsonParam = "";
  WiFiClient client;
  HTTPClient http;  //Declare an object of class HTTPClient
  http.setTimeout(5000);    // 5 Seconds   (could be long with google)
  D_println(aUri);
  http.begin(client, aUri); //Specify request destination

  int httpCode = http.GET();                                  //Send the request
  /*** HTTP client errors
     #define HTTPCLIENT_DEFAULT_TCP_TIMEOUT (5000)
    #define HTTPC_ERROR_CONNECTION_FAILED   (-1)
    #define HTTPC_ERROR_SEND_HEADER_FAILED  (-2)
    #define HTTPC_ERROR_SEND_PAYLOAD_FAILED (-3)
    #define HTTPC_ERROR_NOT_CONNECTED       (-4)
    #define HTTPC_ERROR_CONNECTION_LOST     (-5)
    #define HTTPC_ERROR_NO_STREAM           (-6)
    #define HTTPC_ERROR_NO_HTTP_SERVER      (-7)
    #define HTTPC_ERROR_TOO_LESS_RAM        (-8)
    #define HTTPC_ERROR_ENCODING            (-9)
    #define HTTPC_ERROR_STREAM_WRITE        (-10)
    #define HTTPC_ERROR_READ_TIMEOUT        (-11)
    ****/
  if (httpCode < 0) {
  Serial.print(F("cant get an answer :( http.GET()="));
    Serial.println(httpCode);
    http.end();   //Close connection
    return (false);
  }
  if (httpCode != 200) {
  Serial.print(F("got an error in http.GET() "));
    D_println(httpCode);
    http.end();   //Close connection
    return (false);
  }


  aUri = http.getString();   //Get the request response payload
  //D_println(helperFreeRam() + 1);
  http.end();   //Close connection (restore 22K of ram)
  D_println(aUri);             //Print the response payload
  //D_println(bigString);
  // check json string without real json lib  not realy good but use less memory and faster
  int16_t answerPos = aUri.indexOf(F(",\"answer\":{"));
  if ( !aUri.startsWith(F("{\"status\":true,")) || answerPos < 0 ) {
    return (false);
  }
  // hard cut of "answer":{ xxxxxx } //
  jsonParam = aUri.substring(answerPos + 10, aUri.length() - 1);
  D_println(jsonParam);
  return (true);
}

#define Hex2Char(X) (char)((X) + ((X) <= 9 ? '0' : ('A' - 10)))

// encode optimisé pour le json
String encodeUri(const String aUri) {
  String answer = "";
  String specialChar = F(".-~_{}[],;:\"\\");
  // uri encode maison :)
  for (uint16_t N = 0; N < aUri.length(); N++) {
    char aChar = aUri[N];
    //TODO:  should I keep " " to "+" conversion ????  save 2 char but oldy
    if (aChar == ' ') {
      answer += '+';
    } else if ( isAlphaNumeric(aChar) ) {
      answer +=  aChar;
    } else if (specialChar.indexOf(aChar) >= 0) {
      answer +=  aChar;
    } else {
      answer +=  '%';
      answer += Hex2Char( aChar >> 4 );
      answer += Hex2Char( aChar & 0xF);
    } // if alpha
  }
  return (answer);
}
