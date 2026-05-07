#pragma once
#ifndef MQTTHandler_h
#define MQTTHandler_h

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "configs.h"

class MQTTHandler {
private:
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    bool _mqttConnected;
    unsigned long _lastPublish;
    unsigned long _lastReconnect;
    unsigned long _reconnectDelay;

    String _broker;
    uint16_t _port;

    String _messageQueue[MQTT_MAX_QUEUE];
    String _topicQueue[MQTT_MAX_QUEUE];
    int _queueHead;
    int _queueTail;
    int _queueCount;

    SemaphoreHandle_t _mutex;
    static MQTTHandler* _instance;
    static void mqttCallback(char* topic, byte* payload, unsigned int length);

    void processCommand(const String& json);
    bool enqueueMessage(const String& topic, const String& payload);
    void flushQueue();

public:
    MQTTHandler();

    void begin(const char* broker = MQTT_BROKER, uint16_t port = MQTT_PORT);
    bool connect();
    void disconnect();
    void loop();

    bool publish(const char* topic, const char* payload);
    bool isConnected() { return _mqttConnected; }

    void publishStatus(int aps, int stations, int totalPkts, int btDevs,
                       int battPct, bool charging, bool sdPresent,
                       int rssi, const String& mode);
    void publishAlert(const String& type, const String& source, const String& target);

    void (*onCommand)(const String& cmd, JsonDocument& params);
};

#endif
