#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <cstdint>
#include <SPI.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

enum CommandType : uint8_t {
  MOTOR1,
  MOTOR2,
  DELAY,
  END
};

const int numDrinks = 6;

//const char *drinkNames[][] = {};

int drinkRecipes[][4] {
  {2, 2, 2, 2},
  {4, 0, 4, 0},
  {0, 4, 0, 4},
  {8, 0, 0, 0},
  {0, 8, 0, 0},
  {0, 0, 8, 0},
};

struct Command {
  CommandType cmd;
  int32_t payload;
};

void SPI_sendObject(const Command &value)
{
  const uint8_t *p = (uint8_t*) &value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    SPI.transfer(*p++);
}
int drinkOrder = 0;


AsyncWebServer server(80);

const char* ssid = "esp";
const char* password = "esp45678";
const char * hostName = "esp";
const int readyPin = D4;

//When the blue pill is done controlling the motors
//and the drink is ready, it will pull the readyPin
//down, signalling that it is ready for another order.
volatile bool robotReady = true;
void readyIRQ() {
  robotReady = true;
}

void setup(){
  //pinMode(LED_BUILTIN, OUTPUT);
  pinMode(readyPin, INPUT_PULLUP);
  attachInterrupt(readyPin, readyIRQ, FALLING);

  SPI.begin();
  digitalWrite(SS, HIGH);
  SPI.beginTransaction(SPISettings(18000000, MSBFIRST, SPI_MODE0));
  SPI.setClockDivider(SPI_CLOCK_DIV8);
  
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  if (!MDNS.begin("esp"))
    Serial.println("mDNS failed!");
  MDNS.addService("http","tcp",80);

  SPIFFS.begin();


  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });


  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setCacheControl("max-age=6000");

  server.onNotFound([](AsyncWebServerRequest *request){
    Serial.printf("NOT_FOUND: ");
    if(request->method() == HTTP_GET)
      Serial.printf("GET");
    else if(request->method() == HTTP_POST)
      Serial.printf("POST");
    else if(request->method() == HTTP_DELETE)
      Serial.printf("DELETE");
    else if(request->method() == HTTP_PUT)
      Serial.printf("PUT");
    else if(request->method() == HTTP_PATCH)
      Serial.printf("PATCH");
    else if(request->method() == HTTP_HEAD)
      Serial.printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
      Serial.printf("OPTIONS");
    else
      Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if(request->contentLength()){
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });
  
  server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
      Serial.printf("UploadStart: %s\n", filename.c_str());
    Serial.printf("%s", (const char*)data);
    if(final)
      Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
      Serial.printf("BodyStart: %u\n", total);
    Serial.printf("%s", (const char*)data);
    if(index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });

  server.on("/order", HTTP_POST, [](AsyncWebServerRequest *request) {
    const char *header = "<html>" \
          "<head>" \
          "<meta http-equiv=\"refresh\" content=\"3;url=index.html\" />" \
          "</head>" \
          "<body>" \
          "<h1>";
    const char *footer = "</h1></body></html>";
    String message;
    if (request->hasParam("selection", true)) {
      if (!robotReady) {
        Serial.println("User made an order, but the robot isn't ready. Rejected.");
        message = "Please be patient while the robot gets ready for the next order.";
      }
      else {
        int s = request->getParam("selection", true)->value().toInt();
        if (s < 1 || s >= numDrinks) {
          Serial.println("Ordered invalid drink");
          message = "You ordered an invalid drink.";
        }
        else {
          Serial.print("Ordered drink ");
          Serial.println(s);
          message = "You ordered a " + s;
          drinkOrder = s;
        }
        robotReady = false;
      }
    }
    else {
      Serial.println("'/order': malformed request");
      message = "Error: malformed order";
    }
    request->send(200, "text/html", header + message + footer);
  });
  
  server.begin();
}

void sendCommand(CommandType cmd, int32_t p) {
  Command c;
  c.cmd = cmd;
  c.payload = p;
  digitalWrite(SS, LOW);
  SPI_sendObject(c);
  digitalWrite(SS, HIGH);
}
//Good rule of thumb: spend no more than 20 ms in the loop
//to yield control the the WiFi and TCP stack so it can
//do its thing.
void loop() {
  if (drinkOrder == 0) {
    ;
  }
  else {
    sendCommand(MOTOR1, -1000);
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < drinkRecipes[drinkOrder][i]; j++) {
        sendCommand(MOTOR2, 500);
        sendCommand(DELAY, 100);
        sendCommand(MOTOR2, -500);
      }
      sendCommand(MOTOR1, 200);
    }
    sendCommand(MOTOR1, 450);
    sendCommand(END, 0);
    drinkOrder = 0;
    Serial.println("Sent order commands.");
  }
}
