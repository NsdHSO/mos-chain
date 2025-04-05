#ifndef ESP8266LAPP_H
#define ESP8266LAPP_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <PubSubClient.h>
#include "../include/secrets.h"
#include <SSD1306Wire.h>

class ESP8266LApp {
private:
    // Pin definitions
    static const int D1 = 5;  // SCL on GPIO5
    static const int D2 = 4;  // SDA on GPIO4
    static const int led = LED_BUILTIN;

    // Network credentials
    const char* ssid = WIFI_SSID;
    const char* password = WIFI_PASS;

    // MQTT settings
    const char* mqtt_server = "192.168.68.54";
    const char* mqtt_topic = "test/t";
    const int mqtt_port = 1883;
    String mqtt_client_id;

    // Objects
    ESP8266WebServer server;
    WiFiClient espClient;
    PubSubClient client;
    SSD1306Wire display;
    // Private methods
    void callback(char* topic, byte* payload, unsigned int length);
    void printMQTTState(int state);
    void reconnect();
    void handleRoot();
    void handleNotFound();
    void simpleMessage();
    String urlDecode(String string);

    void handleUpdateCall();
    void printMessage(const String& topic);

public:
    ESP8266LApp();
    void begin();
    void run();
};

#endif
