#pragma once

#include <Arduino.h>

#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include <DNSServer.h>
#include "config/config.h"

#include <ElegantOTA.h>
#include <WiFi.h>
#include "fs/fs.h"
#include "log/logger.h"

class CaptivePortal {
   public:
    CaptivePortal(const char*, const char*);
    ~CaptivePortal();

    void initialize();
    void loop();

    bool credentialsSaved = false;

   protected:
    void start_soft_ap();
    void start_dns();
    void start_web();

    const char* ssid;
    const char* password;

    const char* html = nullptr;
    const char* css = nullptr;

    DNSServer* dnsServer = nullptr;
    AsyncWebServer* server = nullptr;

    /// @brief IP Address to the web server, Samsung requires the IP to be in public space
    const IPAddress accessPointIP = IPAddress(192, 168, 4, 1);
    /// @brief Basic Subnet Mask
    const IPAddress subnetMask = IPAddress(255, 255, 255, 0);

    /// @brief A link to the local IP address with an HTTP protocol
    const String localIPURL = "http://192.168.4.1";
};