#include "ESP8266LApp.h"
#include <Arduino.h>  // Essential for core Arduino functions
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <SSD1306Wire.h>  // Changed from SSD1306.h
#include <PubSubClient.h>
#include <ArduinoJson.h> // Include the ArduinoJson library

ESP8266LApp::ESP8266LApp() : server(80),
                             client(espClient),
                             display(0x3C, D2, D1) {
    mqtt_client_id = "esp8266_" + String(ESP.getChipId());
}

void ESP8266LApp::begin() {
    pinMode(led, OUTPUT);
    digitalWrite(led, 0);
    Serial.begin(115200);

    // Initialize OLED
    display.init();
    printMessage("Connecting to WiFi...");

    // Connect to WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("");
    Serial.print("Connecting to WiFi");

    // Wait for connection
    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
        delay(500);
        Serial.print(".");
        wifiAttempts++;
    }
    Serial.println("");

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Connected to ");
        Serial.println(ssid);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        printMessage("WiFi Connected\nIP: " + WiFi.localIP().toString());
        delay(2000);
    } else {
        Serial.println("Failed to connect to WiFi!");
        printMessage("WiFi Connection\nFailed!");
        delay(2000);
    }

    // Set up MQTT
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback([this](char *topic, byte *payload, unsigned int length) {
        this->callback(topic, payload, length);
    });

    // Try to ping the MQTT broker
    Serial.print("Pinging MQTT broker: ");
    IPAddress mqtt_ip;
    if (WiFi.hostByName(mqtt_server, mqtt_ip)) {
        Serial.print("DNS lookup successful. IP: ");
        Serial.println(mqtt_ip.toString());

        if (espClient.connect(mqtt_ip, mqtt_port)) {
            Serial.println("TCP connection successful!");
            espClient.stop();

            printMessage("MQTT Broker Found\nIP: " + mqtt_ip.toString());
            delay(2000);
        } else {
            Serial.println("TCP connection failed!");

            printMessage("MQTT Broker\nTCP Connection Failed!");
            delay(2000);
        }
    } else {
        Serial.println("DNS lookup failed!");

        printMessage("MQTT Broker\nDNS Lookup Failed!");
        delay(2000);
    }

    // Set up MDNS if WiFi is connected
    if (WiFi.status() == WL_CONNECTED) {
        if (MDNS.begin("esp8266")) {
            Serial.println("MDNS responder started");
        }
    }

    // Set up web server routes
    server.on("/", [this]() { this->handleRoot(); });
    server.on("/update", [this]() { this->handleUpdateCall(); });
    server.on("/simple", [this]() { this->simpleMessage(); });
    server.on("/inline", [this]() {
        server.send(200, "text/plain", "this works as well");
    });
    server.onNotFound([this]() { this->handleNotFound(); });
    server.begin();


    // Try to connect to MQTT
    if (WiFi.status() == WL_CONNECTED) {
        display.clear();
        display.drawString(display.getWidth() / 2, display.getHeight() / 2, "Connecting to MQTT...");
        display.display();
        reconnect();
    }

    // Display web server started
    display.clear();
    display.drawString(display.getWidth() / 2, display.getHeight() / 2,
                       "HTTP Server Started:\n" + WiFi.localIP().toString());
    display.display();
    delay(2000);
}


void ESP8266LApp::run() {
    // Handle HTTP server
    server.handleClient();

    // Handle MQTT connection and loop
    if (WiFi.status() == WL_CONNECTED) {
        if (!client.connected()) {
            static unsigned long lastReconnectAttempt = 0;
            unsigned long now = millis();

            // Try to reconnect every 30 seconds
            if (now - lastReconnectAttempt > 30000) {
                lastReconnectAttempt = now;
                reconnect();
            }
        } else {
            client.loop();
        }
    }
}

void ESP8266LApp::callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    // First loop to build the message string
    for (unsigned int i = 0; i < length; i++) {
        Serial.print(static_cast<unsigned char>(payload[i]));
    }
    Serial.println();

    // Display the message on OLED
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char) payload[i];
    }
    display.clear();
    display.drawString(display.getWidth() / 2, display.getHeight() / 2, message);
    display.display();
}

void ESP8266LApp::printMQTTState(int state) {
    switch (state) {
        case -4: Serial.println("MQTT_CONNECTION_TIMEOUT");
            break;
        case -3: Serial.println("MQTT_CONNECTION_LOST");
            break;
        case -2: Serial.println("MQTT_CONNECT_FAILED");
            break;
        case -1: Serial.println("MQTT_DISCONNECTED");
            break;
        case 0: Serial.println("MQTT_CONNECTED");
            break;
        case 1: Serial.println("MQTT_CONNECT_BAD_PROTOCOL");
            break;
        case 2: Serial.println("MQTT_CONNECT_BAD_CLIENT_ID");
            break;
        case 3: Serial.println("MQTT_CONNECT_UNAVAILABLE");
            break;
        case 4: Serial.println("MQTT_CONNECT_BAD_CREDENTIALS");
            break;
        case 5: Serial.println("MQTT_CONNECT_UNAUTHORIZED");
            break;
        default: Serial.println("UNKNOWN STATE");
            break;
    }
}


void ESP8266LApp::reconnect() {
    int attempts = 0;

    // Try to connect to MQTT server 3 times
    while (!client.connected() && attempts < 3) {
        Serial.print("Attempting MQTT connection... ");
        Serial.print("Client ID: ");
        Serial.println(mqtt_client_id);

        if (espClient.connect(mqtt_server, mqtt_port)) {
            Serial.println("TCP connection successful, now trying MQTT...");

            // Attempt to connect with the unique client ID
            if (client.connect(mqtt_client_id.c_str())) {
                Serial.println("MQTT connected!");
                // Subscribe to the topic
                client.subscribe(mqtt_topic);

                // Display MQTT connected on OLED
                display.clear();
                display.drawString(display.getWidth() / 2, display.getHeight() / 2,
                                   "MQTT Connected\nTopic: " + String(mqtt_topic));
                display.display();
            } else {
                int state = client.state();
                Serial.print("MQTT connection failed, rc=");
                Serial.print(state);
                Serial.print(" - ");
                printMQTTState(state);

                // Show error on OLED
                display.clear();
                display.drawString(display.getWidth() / 2, display.getHeight() / 2, "MQTT Error: " + String(state));
                display.display();
            }
        } else {
            Serial.println("TCP connection to MQTT server failed!");

            // Display TCP error on OLED
            display.clear();
            display.drawString(display.getWidth() / 2, display.getHeight() / 2, "TCP Error\nCan't reach broker");
            display.display();
        }

        attempts++;
        if (!client.connected() && attempts < 3) {
            Serial.println("Waiting 5 seconds before retry...");
            delay(5000);
        }
    }
}

void ESP8266LApp::handleRoot() {
    digitalWrite(led, 1);
    server.send(200, "text/plain", "hello from esp8266!");
    digitalWrite(led, 0);
}



void ESP8266LApp::simpleMessage()  {
    String message = "";

    // Message parameter wasn't found
    if (server.arg("message") == "") {
        message = "Message Argument not found";
    } else {
        // message was found
        message = server.arg("message");

        // Display on OLED
        printMessage(message);
    }

    server.send(200, "text/plain", "OK");
}

void ESP8266LApp::handleNotFound() {
    digitalWrite(led, 1);
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
    digitalWrite(led, 0);
}

void ESP8266LApp::handleUpdateCall() {
    String message = "";

    if (server.arg("message") == "") {
        message = "Message Argument not found";
        server.send(200, "text/plain", "OK");
        return;
    }

    message = urlDecode(server.arg("message")); // Get the raw JSON string

    // Parse the JSON message
    StaticJsonDocument<256> doc; // Adjust size based on your JSON data
    DeserializationError error = deserializeJson(doc, message);
    Serial.printf("your message is: %s\n", message.c_str());
    if (error) {
        // Check for parsing errors
        String errorMsg = "JSON Parsing Failed: ";
        errorMsg += error.c_str();
        server.send(400, "text/plain", errorMsg);
        return;
    }

    // Extract the "topic" field
    const char *extractedTopic = doc["topic"];
    if (!extractedTopic) {
        // Check if "topic" exists
        server.send(400, "text/plain", "Missing 'topic' field in JSON");
        return;
    }

    // Extract the "payload" (optional)
    const char *extractedPayload = doc["payload"];
    String payloadToSend = extractedPayload ? String(extractedPayload) : "";

    // Display the payload on OLED (or use the full message)
    printMessage(payloadToSend);

    // Publish to MQTT
    if (client.connected()) {
        Serial.print("Publishing to topic: ");
        Serial.println(extractedTopic);
        Serial.print("Payload: ");
        Serial.println(payloadToSend);

        // Publish the extracted payload to the extracted topic

        if (!extractedTopic) {
            client.publish(mqtt_topic, message.c_str());
        } else {
            client.publish(extractedTopic, message.c_str());
        }
    } else {
        Serial.println("MQTT not connected, cannot publish");
    }

    server.send(200, "text/plain", "OK");
}

void ESP8266LApp::printMessage(const String &message) {
    display.clear();

    // Convert to String for length check (optional)

    // Use the largest font that fits the message
    if (message.length() <= 5) {
        // Short messages
        display.setFont(ArialMT_Plain_24);
    } else if (message.length() <= 20) {
        // Medium messages
        display.setFont(ArialMT_Plain_16);
    } else {
        // Long messages
        display.setFont(ArialMT_Plain_10);
    }

    // Calculate position to center text
    display.setTextAlignment(TEXT_ALIGN_CENTER);

    // For SSD1306Wire (OLED display) the correct method is getHeight() not getFontHeight()
    int16_t y = (display.getHeight() - 10) / 2; // 10 is approximate font height

    // Draw centered string (simpler with TEXT_ALIGN_CENTER)
    display.drawString(display.getWidth() / 2, y, message);
    display.display();
}
// URL decoding function (from earlier)
String ESP8266LApp::urlDecode(String input) {
    input.replace("+", " ");
    String decoded = "";
    char temp[] = "00";
    for (size_t i = 0; i < input.length(); i++) {
        if (input[i] == '%' && i + 2 < input.length()) {
            temp[0] = input[i + 1];
            temp[1] = input[i + 2];
            decoded += (char)strtol(temp, NULL, 16);
            i += 2;
        } else {
            decoded += input[i];
        }
    }
    return decoded;
}
