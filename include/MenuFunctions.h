#pragma once
#ifndef MenuFunctions_h
#define MenuFunctions_h

#include <Arduino.h>
#include <LinkedList.h>
#include "configs.h"
#include "display.h"
#include "WiFiScan.h"
#include "Switches.h"
#include "EvilPortal.h"
#include "MQTTHandler.h"
#include "SDInterface.h"
#include "BatteryInterface.h"
#include "settings.h"

class MenuFunctions {
private:
    DisplayManager* _display;
    WiFiScanEngine* _wifi;
    EvilPortal* _portal;
    MQTTHandler* _mqtt;
    SDInterface* _sd;
    BatteryInterface* _battery;
    SettingsManager* _settings;

    ButtonManager _buttons;

    // Menu tree
    MenuNode* _mainMenu;
    MenuNode* _currentMenu;
    MenuNode* _wifiMenu;
    MenuNode* _wifiSniffMenu;
    MenuNode* _wifiScanMenu;
    MenuNode* _wifiAttackMenu;
    MenuNode* _wifiGeneralMenu;
    MenuNode* _btMenu;
    MenuNode* _btSniffMenu;
    MenuNode* _btAttackMenu;
    MenuNode* _deviceMenu;
    MenuNode* _appsMenu;

    int _selectedIndex;
    int _scrollOffset;
    int _menuItemCount;
    bool _inScanMode;

    void buildMenuTree();
    MenuNode* createNode(const String& label, WiFiScanMode mode, bool hasSub);
    void addChild(MenuNode* parent, MenuNode* child);

    void renderCurrentMenu();
    void executeSelection();
    void navigateBack();

    void handleScanButtons();

public:
    MenuFunctions();
    void begin(DisplayManager* disp, WiFiScanEngine* wifi, EvilPortal* portal,
               MQTTHandler* mqtt, SDInterface* sd, BatteryInterface* batt,
               SettingsManager* settings);
    void update();
    void buttonTask();
};

#endif
