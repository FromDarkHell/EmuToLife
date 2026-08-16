
#include "web/captive_portal.h"

CaptivePortal::CaptivePortal(const char* ssid, const char* password) {
    this->ssid = ssid;
    this->password = password;
    this->html = fs_read("/portal.html");
    this->css = fs_read("/style.css");

    this->initialize();
}

CaptivePortal::~CaptivePortal() {
    if (dnsServer) {
        dnsServer->stop();
        delete dnsServer;
        dnsServer = nullptr;
    }
    if (server) {
        server->end();
        delete server;
        server = nullptr;
    }
    if (html) {
        free((void*)html);
        html = nullptr;
    }
    if (css) {
        free((void*)css);
        css = nullptr;
    }
}

void CaptivePortal::initialize() {
    this->start_soft_ap();
    this->start_dns();
    this->start_web();
}

void CaptivePortal::loop() {
    if (dnsServer != nullptr) {
        dnsServer->processNextRequest();
        delay(30);
    }

    ElegantOTA.loop();
}

void CaptivePortal::start_soft_ap() {
    WiFi.mode(WIFI_AP);
    WiFi.config(this->accessPointIP, this->accessPointIP, this->subnetMask);
    wl_status_t status = (wl_status_t)WiFi.beginAP(this->ssid, this->password, 6);

    log_dbg("[CaptivePortal::start_soft_ap] Access point started (SSID: %s) (Status: %d)",
            this->ssid, status);
}

void CaptivePortal::start_dns() {
    dnsServer = new DNSServer();

    // Set the TTL for DNS response and start the DNS server
    dnsServer->setTTL(3600);
    bool success = dnsServer->start(53, "*", this->accessPointIP);
    if (!success) {
        log_err("[CaptivePortal::start_dns] Failed to start DNS server");
        return;
    }
    log_dbg("[CaptivePortal::start_dns] DNS server started");
}

void CaptivePortal::start_web() {
    server = new AsyncWebServer(80);
    // Required
    server->on("/connecttest.txt", [](AsyncWebServerRequest* request) {
        request->redirect("http://logout.net");
    });  // windows 11 captive portal workaround
    server->on("/wpad.dat", [](AsyncWebServerRequest* request) {
        request->send(404);
    });  // Honestly don't understand what this is but a 404 stops win 10 keep calling this
         // repeatedly and panicking the esp32 :)

    // Background responses: Probably not all are Required, but some are. Others might speed things
    // up? A Tier (commonly used by modern systems)
    server->on("/generate_204", [this](AsyncWebServerRequest* request) {
        request->redirect(this->localIPURL);
    });  // android captive portal redirect
    server->on("/redirect", [this](AsyncWebServerRequest* request) {
        request->redirect(this->localIPURL);
    });  // microsoft redirect
    server->on("/hotspot-detect.html", [this](AsyncWebServerRequest* request) {
        request->redirect(this->localIPURL);
    });  // apple call home
    server->on("/canonical.html", [this](AsyncWebServerRequest* request) {
        request->redirect(this->localIPURL);
    });  // firefox captive portal call home
    server->on("/success.txt", [this](AsyncWebServerRequest* request) {
        request->send(200);
    });  // firefox captive portal call home
    server->on("/ncsi.txt", [this](AsyncWebServerRequest* request) {
        request->redirect(this->localIPURL);
    });  // windows call home

    // Return 404 to webpage icon
    server->on("/favicon.ico", [](AsyncWebServerRequest* request) { request->send(404); });

    // Serve Basic HTML Page
    server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/css", this->css);
    });
    server->on("/", HTTP_ANY, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/html", this->html);
    });

    // Used for storing / saving config data
    server->on("/connect", HTTP_POST, [this](AsyncWebServerRequest* request) {
        const AsyncWebParameter* ssidParam = nullptr;
        const AsyncWebParameter* passwordParam = nullptr;

        for (int i = 0; i < request->params(); i++) {
            const AsyncWebParameter* param = request->getParam(i);

            if (param->name() == "ssid") {
                ssidParam = param;
            } else if (param->name() == "password") {
                passwordParam = param;
            }
        }

        if (ssidParam != nullptr) {
            config.setString("ssid", ssidParam->value().c_str());
            const char* wifi_password = "\0";
            if (passwordParam != nullptr && !passwordParam->value().isEmpty()) {
                wifi_password = passwordParam->value().c_str();
            }

            config.setString("password", wifi_password);
            config.setBool("initialized", true);

            this->credentialsSaved = true;

            request->send(200, "text/html", "Complete! Plese wait!");
            rp2040.reboot();
        } else {
            request->redirect("/");
        }
    });

    // the catch all
    server->onNotFound(
        [this](AsyncWebServerRequest* request) { request->redirect(this->localIPURL); });

    server->begin();

    ElegantOTA.begin(server);
}