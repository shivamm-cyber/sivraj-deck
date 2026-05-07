/*=============================================================
 * MenuFunctions.cpp — Hierarchical menu system
 *=============================================================*/
#include "MenuFunctions.h"

MenuFunctions::MenuFunctions()
    : _display(nullptr), _wifi(nullptr), _portal(nullptr)
    , _mqtt(nullptr), _sd(nullptr), _battery(nullptr), _settings(nullptr)
    , _mainMenu(nullptr), _currentMenu(nullptr)
    , _selectedIndex(0), _scrollOffset(0), _menuItemCount(0)
    , _inScanMode(false)
{}

void MenuFunctions::begin(DisplayManager* disp, WiFiScanEngine* wifi,
                           EvilPortal* portal, MQTTHandler* mqtt,
                           SDInterface* sd, BatteryInterface* batt,
                           SettingsManager* settings) {
    _display = disp;
    _wifi = wifi;
    _portal = portal;
    _mqtt = mqtt;
    _sd = sd;
    _battery = batt;
    _settings = settings;

    _buttons.begin();
    buildMenuTree();
    _currentMenu = _mainMenu;
    _selectedIndex = 0;
    _scrollOffset = 0;
    Serial.println(F("[MENU] Ready"));
}

// ── Menu Tree Construction ──────────────────────────────────

MenuNode* MenuFunctions::createNode(const String& label, WiFiScanMode mode, bool hasSub) {
    MenuNode* n = new MenuNode();
    n->label = label;
    n->scanMode = mode;
    n->hasSubmenu = hasSub;
    n->parent = nullptr;
    return n;
}

void MenuFunctions::addChild(MenuNode* parent, MenuNode* child) {
    child->parent = parent;
    parent->children.add(child);
}

void MenuFunctions::buildMenuTree() {
    _mainMenu = createNode("SIVRAJ DECK", WIFI_SCAN_OFF, true);

    // ── WiFi Menu ───────────────────────────────
    _wifiMenu = createNode("WiFi", WIFI_SCAN_OFF, true);
    addChild(_mainMenu, _wifiMenu);

    _wifiSniffMenu = createNode("Sniffers", WIFI_SCAN_OFF, true);
    addChild(_wifiMenu, _wifiSniffMenu);
    addChild(_wifiSniffMenu, createNode("Beacon Sniff", WIFI_SCAN_BEACON_SNIFF, false));
    addChild(_wifiSniffMenu, createNode("Deauth Sniff", WIFI_SCAN_DEAUTH_SNIFF, false));
    addChild(_wifiSniffMenu, createNode("Probe Sniff", WIFI_SCAN_PROBE, false));
    addChild(_wifiSniffMenu, createNode("EAPOL/PMKID", WIFI_SCAN_EAPOL, false));
    addChild(_wifiSniffMenu, createNode("Pkt Monitor", WIFI_SCAN_PACKET_MONITOR, false));
    addChild(_wifiSniffMenu, createNode("Raw Capture", WIFI_SCAN_RAW, false));
    addChild(_wifiSniffMenu, createNode("Pineapple", WIFI_SCAN_PINEAPPLE, false));

    _wifiScanMenu = createNode("Scanners", WIFI_SCAN_OFF, true);
    addChild(_wifiMenu, _wifiScanMenu);
    addChild(_wifiScanMenu, createNode("Scan APs", WIFI_SCAN_AP, false));
    addChild(_wifiScanMenu, createNode("Scan Stations", WIFI_SCAN_STATION, false));
    addChild(_wifiScanMenu, createNode("Scan All", WIFI_SCAN_ALL, false));
    addChild(_wifiScanMenu, createNode("Channel Analyze", WIFI_SCAN_CHANNEL_ANALYZER, false));
    addChild(_wifiScanMenu, createNode("Ping Sweep", WIFI_SCAN_PING, false));
    addChild(_wifiScanMenu, createNode("ARP Scan", WIFI_SCAN_ARP, false));
    addChild(_wifiScanMenu, createNode("Port Scan", WIFI_SCAN_PORT, false));

    _wifiAttackMenu = createNode("Attacks", WIFI_SCAN_OFF, true);
    addChild(_wifiMenu, _wifiAttackMenu);
    addChild(_wifiAttackMenu, createNode("Deauth Flood", WIFI_ATTACK_DEAUTH_FLOOD, false));
    addChild(_wifiAttackMenu, createNode("Beacon Spam", WIFI_ATTACK_BEACON_LIST, false));
    addChild(_wifiAttackMenu, createNode("Random Beacon", WIFI_ATTACK_BEACON_RANDOM, false));
    addChild(_wifiAttackMenu, createNode("Rick Roll", WIFI_ATTACK_RICKROLL, false));
    addChild(_wifiAttackMenu, createNode("Probe Flood", WIFI_ATTACK_PROBE_FLOOD, false));
    addChild(_wifiAttackMenu, createNode("AP Clone", WIFI_ATTACK_AP_CLONE, false));

    _wifiGeneralMenu = createNode("General", WIFI_SCAN_OFF, true);
    addChild(_wifiMenu, _wifiGeneralMenu);
    addChild(_wifiGeneralMenu, createNode("Join WiFi", WIFI_SCAN_OFF, false));
    addChild(_wifiGeneralMenu, createNode("Clear APs", WIFI_SCAN_OFF, false));
    addChild(_wifiGeneralMenu, createNode("Clear Stations", WIFI_SCAN_OFF, false));
    addChild(_wifiGeneralMenu, createNode("Set MAC", WIFI_SCAN_OFF, false));

    // ── BT Menu ─────────────────────────────────
    _btMenu = createNode("Bluetooth", WIFI_SCAN_OFF, true);
    addChild(_mainMenu, _btMenu);
    addChild(_btMenu, createNode("BLE Scan", WIFI_SCAN_OFF, false));
    addChild(_btMenu, createNode("BT Sniff", WIFI_SCAN_OFF, false));
    addChild(_btMenu, createNode("BLE Spam", WIFI_SCAN_OFF, false));

    // ── Apps Menu ───────────────────────────────
    _appsMenu = createNode("Apps", WIFI_SCAN_OFF, true);
    addChild(_mainMenu, _appsMenu);
    addChild(_appsMenu, createNode("Evil Portal", WIFI_SCAN_OFF, false));
    addChild(_appsMenu, createNode("SSID List", WIFI_SCAN_OFF, false));
    addChild(_appsMenu, createNode("Generate SSIDs", WIFI_SCAN_OFF, false));

    // ── Device Menu ─────────────────────────────
    _deviceMenu = createNode("Device", WIFI_SCAN_OFF, true);
    addChild(_mainMenu, _deviceMenu);
    addChild(_deviceMenu, createNode("Device Info", WIFI_SCAN_OFF, false));
    addChild(_deviceMenu, createNode("Settings", WIFI_SCAN_OFF, false));
    addChild(_deviceMenu, createNode("SD Card", WIFI_SCAN_OFF, false));
    addChild(_deviceMenu, createNode("Update FW", WIFI_SCAN_OFF, false));
    addChild(_deviceMenu, createNode("Reboot", WIFI_SCAN_OFF, false));
}

// ── Rendering ───────────────────────────────────────────────

void MenuFunctions::renderCurrentMenu() {
    if (!_display || !_currentMenu) return;

    _menuItemCount = _currentMenu->children.size();
    const char* items[_menuItemCount];

    for (int i = 0; i < _menuItemCount; i++) {
        items[i] = _currentMenu->children.get(i)->label.c_str();
    }

    display.clear();
    display.drawStatusBar(
        wifiEngine.getChannel(), wifiEngine.getAPCount(),
        wifiEngine.getStationCount(), wifiEngine.getPacketsPerSec(),
        _battery ? _battery->getPercent() : 0,
        _battery ? _battery->isCharging() : false,
        _sd ? _sd->isReady() : false,
        _mqtt ? _mqtt->isConnected() : false
    );
    display.drawMenuHeader(_currentMenu->label.c_str());
    display.drawMenuItems(items, _menuItemCount, _selectedIndex, _scrollOffset);
    display.pushSprite();
}

// ── Execution ───────────────────────────────────────────────

void MenuFunctions::executeSelection() {
    if (!_currentMenu || _selectedIndex >= _currentMenu->children.size()) return;

    MenuNode* sel = _currentMenu->children.get(_selectedIndex);

    if (sel->hasSubmenu) {
        _currentMenu = sel;
        _selectedIndex = 0;
        _scrollOffset = 0;
        return;
    }

    // Special label-based actions
    if (sel->label == "Reboot") { ESP.restart(); return; }
    if (sel->label == "Clear APs") { wifiEngine.clearAPs(); return; }
    if (sel->label == "Clear Stations") { wifiEngine.clearStations(); return; }
    if (sel->label == "Evil Portal") {
        if (_portal) { _portal->begin(); }
        _inScanMode = true; return;
    }
    if (sel->label == "Generate SSIDs") { wifiEngine.generateSSIDs(20); return; }
    if (sel->label == "Device Info") { _inScanMode = true; return; }

    WiFiScanMode mode = sel->scanMode;
    if (mode != WIFI_SCAN_OFF) {
        wifiEngine.startScan(mode);
        _inScanMode = true;
    }
}

void MenuFunctions::navigateBack() {
    if (_inScanMode) {
        wifiEngine.stopScan();
        _inScanMode = false;
        return;
    }
    if (_currentMenu && _currentMenu->parent) {
        _currentMenu = _currentMenu->parent;
        _selectedIndex = 0;
        _scrollOffset = 0;
    }
}

// ── Scan Display ────────────────────────────────────────────

void MenuFunctions::handleScanButtons() {
    _buttons.update();

    if (_buttons.evtUp == BTN_LONG_PRESS) {
        navigateBack();
        return;
    }

    // While scanning, show live data
    if (_inScanMode) {
        display.clear();
        display.drawStatusBar(
            wifiEngine.getChannel(), wifiEngine.getAPCount(),
            wifiEngine.getStationCount(), wifiEngine.getPacketsPerSec(),
            _battery ? _battery->getPercent() : 0,
            _battery ? _battery->isCharging() : false,
            _sd ? _sd->isReady() : false,
            _mqtt ? _mqtt->isConnected() : false
        );

        WiFiScanMode m = wifiEngine.getCurrentMode();
        if (m == WIFI_SCAN_PACKET_MONITOR) {
            display.drawPacketMonitor(wifiEngine.getPacketsPerSec(),
                wifiEngine.getTotalPackets(), wifiEngine.getChannel(),
                wifiEngine.getAPCount(), wifiEngine.getStationCount());
        } else if (m == WIFI_SCAN_CHANNEL_ANALYZER) {
            display.drawChannelGraph(wifiEngine.getChannelData());
        } else if (m == WIFI_SCAN_EAPOL) {
            display.drawEAPOLCapture(wifiEngine.getHandshakes(),
                                      wifiEngine.getEapolCount());
        } else if (m >= WIFI_ATTACK_DEAUTH_FLOOD) {
            display.drawAttackStatus("ATTACK ACTIVE",
                wifiEngine.getAttackPktCount(), millis(), true);
        } else {
            display.drawScanProgress("Scanning...", "Promiscuous mode",
                wifiEngine.getAPCount(), millis());
        }
        display.pushSprite();
    }
}

void MenuFunctions::buttonTask() {
    _buttons.update();
}

// ── Main Update Loop ────────────────────────────────────────

void MenuFunctions::update() {
    if (_inScanMode) {
        handleScanButtons();
        return;
    }

    _buttons.update();

    if (_buttons.evtUp == BTN_LONG_PRESS) {
        navigateBack();
    } else if (_buttons.evtUp == BTN_SHORT_PRESS) {
        if (_selectedIndex > 0) {
            _selectedIndex--;
            if (_selectedIndex < _scrollOffset) _scrollOffset = _selectedIndex;
        }
    }

    if (_buttons.evtDown == BTN_SHORT_PRESS) {
        if (_selectedIndex < _menuItemCount - 1) {
            _selectedIndex++;
            if (_selectedIndex >= _scrollOffset + MENU_ITEMS_VISIBLE)
                _scrollOffset = _selectedIndex - MENU_ITEMS_VISIBLE + 1;
        }
    }

    if (_buttons.evtSelect == BTN_SHORT_PRESS) {
        executeSelection();
    }

    renderCurrentMenu();
}
