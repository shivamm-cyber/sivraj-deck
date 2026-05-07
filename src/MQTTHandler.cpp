/*=============================================================
 * MQTTHandler.cpp — MQTT relay to sivraj.in
 *=============================================================*/
#include "MQTTHandler.h"

MQTTHandler* MQTTHandler::_instance = nullptr;

MQTTHandler::MQTTHandler()
    : _mqttClient(_wifiClient)
    , _mqttConnected(false)
    , _lastPublish(0)
    , _lastReconnect(0)
    , _reconnectDelay(MQTT_RECONNECT_BASE)
    , _queueHead(0)
    , _queueTail(0)
    , _queueCount(0)
    , onCommand(nullptr)
{
    _instance = this;
    _mutex = xSemaphoreCreateMutex();
}

void MQTTHandler::begin(const char* broker, uint16_t port) {
    _broker = String(broker);
    _port = port;
    _mqttClient.setServer(broker, port);
    _mqttClient.setCallback(mqttCallback);
    _mqttClient.setBufferSize(512);
    Serial.printf("[MQTT] Configured: %s:%d\n", broker, port);
}

bool MQTTHandler::connect() {
    if (!WiFi.isConnected()) return false;

    String clientId = "sivraj-deck-" + String(random(0xFFFF), HEX);
    Serial.printf("[MQTT] Connecting as %s...\n", clientId.c_str());

    if (_mqttClient.connect(clientId.c_str())) {
        _mqttConnected = true;
        _reconnectDelay = MQTT_RECONNECT_BASE;
        _mqttClient.subscribe(MQTT_TOPIC_COMMAND);
        Serial.println(F("[MQTT] Connected!"));

        // Announce online
        publish(MQTT_TOPIC_STATUS, "{\"status\":\"online\"}");
        return true;
    }

    Serial.printf("[MQTT] Failed, rc=%d\n", _mqttClient.state());
    return false;
}

void MQTTHandler::disconnect() {
    _mqttClient.disconnect();
    _mqttConnected = false;
}

void MQTTHandler::loop() {
    if (!WiFi.isConnected()) {
        _mqttConnected = false;
        return;
    }

    if (!_mqttClient.connected()) {
        _mqttConnected = false;
        uint32_t now = millis();
        if (now - _lastReconnect >= _reconnectDelay) {
            _lastReconnect = now;
            if (connect()) {
                flushQueue();
            } else {
                // Exponential backoff
                _reconnectDelay = min(_reconnectDelay * 2, (unsigned long)MQTT_RECONNECT_MAX);
            }
        }
        return;
    }

    _mqttConnected = true;
    _mqttClient.loop();
    flushQueue();
}

bool MQTTHandler::publish(const char* topic, const char* payload) {
    if (_mqttClient.connected()) {
        return _mqttClient.publish(topic, payload);
    }
    // Queue if not connected
    return enqueueMessage(String(topic), String(payload));
}

bool MQTTHandler::enqueueMessage(const String& topic, const String& payload) {
    if (!_mutex) return false;
    if (!xSemaphoreTake(_mutex, pdMS_TO_TICKS(100))) return false;

    if (_queueCount >= MQTT_MAX_QUEUE) {
        // Drop oldest
        _queueTail = (_queueTail + 1) % MQTT_MAX_QUEUE;
        _queueCount--;
    }

    _topicQueue[_queueHead] = topic;
    _messageQueue[_queueHead] = payload;
    _queueHead = (_queueHead + 1) % MQTT_MAX_QUEUE;
    _queueCount++;

    xSemaphoreGive(_mutex);
    return true;
}

void MQTTHandler::flushQueue() {
    if (!_mqttClient.connected() || _queueCount == 0) return;
    if (!_mutex) return;
    if (!xSemaphoreTake(_mutex, pdMS_TO_TICKS(100))) return;

    while (_queueCount > 0 && _mqttClient.connected()) {
        _mqttClient.publish(_topicQueue[_queueTail].c_str(),
                           _messageQueue[_queueTail].c_str());
        _queueTail = (_queueTail + 1) % MQTT_MAX_QUEUE;
        _queueCount--;
    }

    xSemaphoreGive(_mutex);
}

void MQTTHandler::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (!_instance) return;

    String msg;
    msg.reserve(length);
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

    Serial.printf("[MQTT] Recv [%s]: %s\n", topic, msg.c_str());
    _instance->processCommand(msg);
}

void MQTTHandler::processCommand(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;

    String cmd = doc["cmd"] | "";
    if (cmd.length() == 0) return;

    Serial.printf("[MQTT] Command: %s\n", cmd.c_str());
    if (onCommand) {
        onCommand(cmd, doc);
    }
}

void MQTTHandler::publishStatus(int aps, int stations, int totalPkts, int btDevs,
                                 int battPct, bool charging, bool sdPresent,
                                 int rssi, const String& mode) {
    JsonDocument doc;
    doc["aps"] = aps;
    doc["stations"] = stations;
    doc["packets"] = totalPkts;
    doc["bt_devices"] = btDevs;
    doc["battery"] = battPct;
    doc["charging"] = charging;
    doc["sd"] = sdPresent;
    doc["rssi"] = rssi;
    doc["mode"] = mode;
    doc["heap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;

    String payload;
    serializeJson(doc, payload);
    publish(MQTT_TOPIC_STATUS, payload.c_str());
}

void MQTTHandler::publishAlert(const String& type, const String& source,
                                const String& target) {
    JsonDocument doc;
    doc["type"] = type;
    doc["source"] = source;
    doc["target"] = target;
    doc["timestamp"] = millis();

    String payload;
    serializeJson(doc, payload);
    publish(MQTT_TOPIC_ALERT, payload.c_str());
}
