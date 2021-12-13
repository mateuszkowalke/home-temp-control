#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <strings.h>

#define RIGHT_PIN 22
#define LEFT_PIN 23

unsigned long duration;
int direction;

const char *ssid = "thermostat";
const char *password = "thermostatpass";
AsyncWebServer server(80);
IPAddress localIP(192, 168, 4, 117);
/* IPAddress gateway(192, 168, 1, 1); */
/* IPAddress subnet(255, 255, 255, 0); */
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void WiFiLostIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("Lost IP");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.disconnected.reason);
  Serial.println("Trying to Reconnect");
  WiFi.begin(ssid, password);
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler(
    "/turn", [](AsyncWebServerRequest *request, JsonVariant &json) {
      Serial.println("Received request from sensor.");
      JsonObject jsonObj = json.as<JsonObject>();
      const char *directionStr = jsonObj.getMember("direction");
      direction = atoi(directionStr);
      const char *durationStr = jsonObj.getMember("duration");
      duration = millis() + atoi(durationStr) * 1000;
      request->send(200, "text/html", "success");
    });

void setup() {
  Serial.begin(115200);

  pinMode(RIGHT_PIN, OUTPUT);
  pinMode(LEFT_PIN, OUTPUT);

  /* if (!WiFi.config(localIP, gateway, subnet, primaryDNS, secondaryDNS)) { */
  /*   Serial.println("STA Failed to configure"); */
  /* } */

  if (!WiFi.config(localIP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiLostIP, SYSTEM_EVENT_STA_LOST_IP);
  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.begin(ssid, password);

  delay(100);

  server.addHandler(handler);
  server.onNotFound(notFound);
  server.begin();
}

void loop() {
  if (duration > millis()) {
    if (direction == 0) {
      digitalWrite(LEFT_PIN, LOW);
      digitalWrite(RIGHT_PIN, HIGH);
    } else {
      digitalWrite(RIGHT_PIN, LOW);
      digitalWrite(LEFT_PIN, HIGH);
    }
  } else {
    digitalWrite(RIGHT_PIN, LOW);
    digitalWrite(LEFT_PIN, LOW);
  }
  delay(10);
}
