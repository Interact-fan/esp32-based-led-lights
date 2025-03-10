#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include "config.h"
#include <esp_wifi.h>

// MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected to WiFi!");
  Serial.print("Local IP Address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32_Brightness", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(brightness_command_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void publishLightState() {
  if (lightOn) {
    client.publish(state_announce_topic, "ON", true);
  } else {
    client.publish(state_announce_topic, "OFF", true);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();

  if (String(topic) == brightness_command_topic) {
    if (message == "ON") {
      lightOn = true;
      ledcWrite(pwmChannel, brightness);
      publishLightState();
      Serial.println("Light turned ON with brightness: " + String(brightness));
    } else if (message == "OFF") {
      lightOn = false;
      ledcWrite(pwmChannel, 0);
      publishLightState();
      Serial.println("Light turned OFF");
    } else {
      int newBrightness = message.toInt();
      if (newBrightness >= 0 && newBrightness <= 255) {
        brightness = newBrightness;
        lightOn = (brightness > 0);
        ledcWrite(pwmChannel, brightness);
        publishLightState();
        client.publish(led_state_topic, String(brightness).c_str(), true);
        Serial.println("Brightness set to: " + String(brightness));
      } else {
        Serial.println("Invalid brightness value received: " + message);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  reconnect();

  publishLightState();
  client.publish(led_state_topic, String(brightness).c_str(), true);

  ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
  ledcAttachPin(pwmPin, pwmChannel);
  if (default_state) {
    ledcWrite(pwmChannel, brightness);
  } else {
    ledcWrite(pwmChannel, 0);
  }

  if (otaEnabled) {
    Serial.println("Setting up OTA");
    ArduinoOTA.setHostname(mdnsName);
    if (strlen(otaPassword) != 0) {
      ArduinoOTA.setPassword(otaPassword);
      Serial.printf("OTA Password set: %s\n", otaPassword);
    } else {
      Serial.println("No OTA password set! (insecure)");
    }
    ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
          type = "sketch";
        } else {
          type = "filesystem";
        }
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd of OTA");
        delay(100);
        ESP.restart();
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        switch (error) {
          case OTA_AUTH_ERROR:
            Serial.println("Auth Failed");
            break;
          case OTA_BEGIN_ERROR:
            Serial.println("Begin Failed");
            break;
          case OTA_CONNECT_ERROR:
            Serial.println("Connect Failed");
            break;
          case OTA_RECEIVE_ERROR:
            Serial.println("Receive Failed");
            break;
          case OTA_END_ERROR:
            Serial.println("End Failed");
            break;
        }
        ESP.restart();
      });
    ArduinoOTA.begin();
  }

  if (!MDNS.begin(mdnsName)) {
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");
}

void loop() {
  if (otaEnabled) {
    ArduinoOTA.handle();
  }
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(100);
}