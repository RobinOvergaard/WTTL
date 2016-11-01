#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include "websites.h"

#define lightPin D0
#define onOffPin D1

const char *ssid = "osaa-g";
const char *password = "deadbeef42";
const char *ssidConfig = "lamp_config";

bool buttonPressed = false;
unsigned long buttonPressedTime = 0;
unsigned long interruptDelay = millis();
int currentLightLevel = 0;
unsigned int targetLightLevel = 1024;
unsigned int fadeDelta = 10;
int fadeDirection = 1;

ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println("ESP starting up...");

  pinMode(lightPin, OUTPUT);
  pinMode(onOffPin, INPUT);
  attachInterrupt(onOffPin, powerButtonInterrupt, FALLING);

  WiFi.softAP(ssidConfig);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  if (!MDNS.begin("esp8266")) {
    Serial.println("Error setting up MDNS responder!");
    while(1) { 
      delay(1000);
    }
  }

  server.on("/action", handleAction);
  // in the notFound we search for files ourselves
  server.onNotFound(handleNotFound);
  server.begin();
}

void processLightLevel() {
  currentLightLevel = currentLightLevel + (fadeDelta * fadeDirection);

  if((currentLightLevel > targetLightLevel && fadeDirection > 0) || (currentLightLevel < targetLightLevel && fadeDirection < 0)) {
    currentLightLevel = targetLightLevel;
    return;
  }

  if(currentLightLevel <= 0) currentLightLevel = 0;
  if(currentLightLevel >= 1024) currentLightLevel = 1024;

  Serial.println(currentLightLevel);
  analogWrite(lightPin, currentLightLevel);
}

bool loadFromFlash(String path) {
  if (path.endsWith("/")) {
    path += "index.html";
  }

  int NumFiles = sizeof(files) / sizeof(struct t_websitefiles);

  for (int i = 0; i < NumFiles; i++) {
    if (path.endsWith(String(files[i].filename))) {
      _FLASH_ARRAY<uint8_t>* filecontent;
      String dataType = "text/plain";
      unsigned int len = 0;

      dataType = files[i].mime;
      len = files[i].len;

      server.setContentLength(len);
      server.send(200, files[i].mime, "");

      filecontent = (_FLASH_ARRAY<uint8_t>*)files[i].content;
      filecontent->open();

      WiFiClient client = server.client();
      client.write(*filecontent, 100);

      return true;
    }
  }

  return false;
}

void powerButtonInterrupt() {
  if(interruptDelay - millis() > 100) {
    interruptDelay = millis();
    if (targetLightLevel != 0) {
      fadeDirection = -1;
      targetLightLevel = 0;
    }
    else if (targetLightLevel == 0) {
      fadeDirection = 1;
      targetLightLevel = 1024;
    }
  }
}

void handleAction() {
  for (int i = 0; i < server.args(); i++) {
    String argName = server.argName(i);

    if (argName == "on") {
      String argValue = server.arg(i);
      int value = argValue.toInt();
      int newTargetLightLevel = map(value, 0, 100, 0, 1024);
      
      if(targetLightLevel < newTargetLightLevel) fadeDirection = 1;
      if(targetLightLevel > newTargetLightLevel) fadeDirection = -1;
      if(targetLightLevel == newTargetLightLevel) fadeDirection = 0;
      
      targetLightLevel = newTargetLightLevel;

      Serial.println((String)targetLightLevel + " : " + (String)fadeDirection);
    }    
  }  

  //server.sendHeader("Location", String("/"), true);
  server.send ( 204, "text/plain", "");
}

void handleNotFound() {
  // try to find the file in the flash
  if (loadFromFlash(server.uri())) {
    return;
  }

  String message = "File Not Found\n\n";

  message += "URI..........: ";
  message += server.uri();
  message += "\nMethod.....: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments..: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  message += "\n";
  message += "FreeHeap.....: " + String(ESP.getFreeHeap()) + "\n";
  message += "ChipID.......: " + String(ESP.getChipId()) + "\n";
  message += "FlashChipId..: " + String(ESP.getFlashChipId()) + "\n";
  message += "FlashChipSize: " + String(ESP.getFlashChipSize()) + " bytes\n";
  message += "getCycleCount: " + String(ESP.getCycleCount()) + " Cycles\n";
  message += "Milliseconds.: " + String(millis()) + " Milliseconds\n";

  server.send(404, "text/plain", message);
}

void loop() {
  // process webserver
  server.handleClient();
  processLightLevel();
}
