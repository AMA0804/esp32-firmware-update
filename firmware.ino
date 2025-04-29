#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <Update.h>

#define WIFI_SSID     "Ama"
#define WIFI_PASSWORD "00000000"

#define MQTT_HOST     "demo.thingsboard.io"
#define MQTT_PORT     1883
#define ACCESS_TOKEN  "39J4Ie25ZKI0q7ajKptu"

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  mqttClient.subscribe("v1/devices/me/firmware", 1);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.println("Firmware Update Triggered!");
  Serial.println("Downloading firmware...");

  WiFiClient client;
  HTTPClient http;
  http.begin(payload); // payload contains the firmware URL
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    bool canBegin = Update.begin(contentLength);

    if (canBegin) {
      WiFiClient * stream = http.getStreamPtr();
      size_t written = Update.writeStream(*stream);

      if (written == contentLength) {
        Serial.println("Written : " + String(written) + " successfully");
      } else {
        Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");
      }

      if (Update.end()) {
        Serial.println("OTA done!");
        if (Update.isFinished()) {
          Serial.println("Update successfully completed. Rebooting.");
          ESP.restart();
        } else {
          Serial.println("Update not finished? Something went wrong.");
        }
      } else {
        Serial.println("Error Occurred. Error #: " + String(Update.getError()));
      }
    } else {
      Serial.println("Not enough space to begin OTA");
    }
  } else {
    Serial.println("Can't download firmware. HTTP error: " + String(httpCode));
  }

  http.end();
}

void setup() {
  Serial.begin(115200);

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent([](WiFiEvent_t event) {
    Serial.println("Wi-Fi connected");
    connectToMqtt();
  }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);

  WiFi.onEvent([](WiFiEvent_t event) {
    Serial.println("Wi-Fi disconnected");
    xTimerStop(mqttReconnectTimer, 0);
    xTimerStart(wifiReconnectTimer, 0);
  }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCredentials(ACCESS_TOKEN, "");

  connectToWifi();
}

void loop() {}
