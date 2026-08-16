#pragma once

#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include <Arduino.h>

#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include <ElegantOTA.h>
#include <LEAmDNS.h>
#include "config/config.h"
#include "fs/fs.h"
#include "lego/toybox.h"
#include "lego/toypad.h"
#include "log/logger.h"

class LegoServer {
   public:
    LegoServer(const char*, const char*, Toypad*);
    ~LegoServer();

    void initialize();
    void loop();

    bool success = false;

   protected:
    bool connect_to_ap();
    bool start_mdns();
    void start_web();

    bool add_lego_endpoints();

    bool load_toybox();
    bool store_toybox();

    const char* ssid;
    const char* password;

    const char* html = nullptr;
    const char* css = nullptr;

    AsyncWebServer* server = nullptr;

    AsyncWebSocket ws{"/ws"};

    Toybox* toybox = nullptr;
    bool _toyboxDirty = false;
    uint32_t _lastStoreMs = 0;
    static constexpr uint32_t STORE_DEBOUNCE_MS = 2000;

    Toypad* toypad = nullptr;
};