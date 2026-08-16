#include "web/server.h"

LegoServer::LegoServer(const char* ssid, const char* password, Toypad* toypadInstance) {
    this->ssid = strdup(ssid);
    this->password = strdup(password);
    this->html = fs_read("/index.html");
    this->css = fs_read("/style.css");
    this->toypad = toypadInstance;

    this->initialize();
}
LegoServer::~LegoServer() {
    free((void*)this->ssid);
    free((void*)this->password);

    free((void*)this->css);
    free((void*)this->html);
}

void LegoServer::initialize() {
    success = true;

    success &= this->connect_to_ap();
    if (!success) {
        return;
    }

    success &= this->start_mdns();
    if (!success) {
        return;
    }

    this->start_web();

    this->load_toybox();
}

bool LegoServer::start_mdns() {
    if (!MDNS.begin("emupad")) {
        log_dbg("[server::start_mdns] Error setting up MDNS!");

        return false;
    }

    log_dbg("[server::start_mdns] Successfully started MDNS!");

    return true;
}

bool LegoServer::connect_to_ap() {
    WiFi.mode(WIFI_STA);

    log_dbg("[server::connect_to_ap] Connecting to '%s'", this->ssid);

    int attempts = 0;
    while (attempts < 5) {
        wl_status_t status = (wl_status_t)WiFi.begin(this->ssid, this->password);

        while (status != WL_CONNECTED) {
            if (status == WL_CONNECT_FAILED) {
                break;
            }

            status = (wl_status_t)WiFi.status();
            delay(500);
        }

        if (status == WL_CONNECTED) {
            break;
        }

        attempts++;
        log_dbg("[server::connect_to_ap] Attempt %d to connect to '%s' failed", attempts,
                this->ssid);
        delay(1000);
    }

    log_dbg("[server::connect_to_ap] Connected to '%s' (IP Address: %s)", this->ssid,
            WiFi.localIP().toString().c_str());

    return true;
}

void LegoServer::start_web() {
    server = new AsyncWebServer(80);

    server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/css", this->css);
    });

    server->on("/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        config.clear();
        request->send(200, "text/html", "Success! Please wait");
        rp2040.reboot();
    });

    server->on("/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/html", "Success! Please wait");
        rp2040.reboot();
    });

    server->on("/logs", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", log_get());
    });

    server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/html", this->html);
    });

    server->on(
        "/upload", HTTP_POST, [](AsyncWebServerRequest* request) {}, nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index,
               size_t total) {
            static String body;
            static String filePath;

            if (index == 0) {
                // Resolve file path from query param, e.g. POST /upload?path=/index.html
                if (request->hasParam("path")) {
                    filePath = request->getParam("path")->value();

                    // Ensure it starts with a leading slash
                    if (!filePath.startsWith("/")) {
                        filePath = "/" + filePath;
                    }
                } else {
                    request->send(400, "text/plain", "Missing 'path' query parameter");
                    return;
                }

                body = "";
                body.reserve(total);
            }

            body += String((char*)data).substring(0, len);

            if (index + len == total) {
                fs_write(filePath.c_str(), body.c_str());

                // Only refresh cached html pointer if the uploaded file is the active HTML page
                if (filePath == "/index.html") {
                    free((void*)this->html);
                    this->html = strdup(body.c_str());
                }
                if (filePath == "/style.css") {
                    free((void*)this->css);
                    this->css = strdup(body.c_str());
                }

                request->send(200, "text/plain", "Uploaded " + filePath);
            }
        });

    server->onNotFound([this](AsyncWebServerRequest* request) {
        String path = request->url();
        // log_dbg("[server] Request for %s", path.c_str());

        if (fs_exists(path.c_str())) {
            String contentType = "";
            if (path.endsWith(".webp")) {
                contentType = "image/webp";
            }

            AsyncWebServerResponse* response = request->beginResponse(LittleFS, path, contentType);

            // Cache static assets for 7 days and mark them as immutable, so that browsers can cache
            // them aggressively
            response->addHeader("Cache-Control", "public, max-age=604800, immutable");

            request->send(response);
        } else {
            request->send(404, "text/html", "Failed to find file");
        }
    });

    this->add_lego_endpoints();

    server->begin();

    ElegantOTA.begin(server);
}

void LegoServer::loop() {
    MDNS.update();
    ElegantOTA.loop();
    ws.cleanupClients();

    if (_toyboxDirty && (millis() - _lastStoreMs > STORE_DEBOUNCE_MS)) {
        store_toybox();
        _toyboxDirty = false;
        _lastStoreMs = millis();
    }
}

bool LegoServer::add_lego_endpoints() {
    server->on("/toybox", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (this->toybox == nullptr) {
            this->load_toybox();
        }
        request->send(200, "application/json", this->toybox->serialize());
    });

    // Running a DELETE request onto the `/toybox` endpoint, adds a new tag to the toybox
    server->on("/toybox-delete", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (this->toybox == nullptr) {
            this->load_toybox();
        }

        const char* tag_uid = nullptr;
        for (int i = 0; i < request->params(); i++) {
            const AsyncWebParameter* param = request->getParam(i);

            if (param->name() == "uid") {
                tag_uid = param->value().c_str();
            } else {
                log_warn("[server::toybox] Unknown param %s", param->name().c_str());
            }
        }

        if (tag_uid == nullptr) {
            request->send(500);
            return;
        }

        log_dbg("[server::toybox] Deleting tag from toybox / UID: %s", tag_uid);

        if (!this->toybox->removeToy(tag_uid)) {
            request->send(404);
            return;
        }

        this->store_toybox();
        request->send(200, "application/json", this->toybox->serialize());
    });

    // Running a PUT request onto the `/toybox` endpoint, adds a new tag to the toybox
    server->on("/toybox", HTTP_PUT, [this](AsyncWebServerRequest* request) {
        if (this->toybox == nullptr) {
            this->load_toybox();
        }

        const char* tag_name = nullptr;
        int tag_id = 0xFFFF;
        const char* tag_type = nullptr;

        for (int i = 0; i < request->params(); i++) {
            const AsyncWebParameter* param = request->getParam(i);

            if (param->name() == "name") {
                tag_name = param->value().c_str();
            } else if (param->name() == "id") {
                tag_id = atoi(param->value().c_str());
            } else if (param->name() == "type") {
                tag_type = param->value().c_str();
            } else {
                log_warn("[server::toybox] Unknown param %s", param->name().c_str());
            }
        }

        log_dbg("[server::toybox] Adding new tag to toybox / Name: %s / ID: %d / Type: %s",
                tag_name, tag_id, tag_type);

        this->toybox->addToy(tag_name, tag_id, tag_type);
        this->store_toybox();
        request->send(200, "application/json", this->toybox->serialize());
    });

    // Running a PUT request onto the `/toypad` endpoint
    // It lets you move a tag around based on the UID.
    server->on("/toypad", HTTP_PUT, [this](AsyncWebServerRequest* request) {
        if (this->toybox == nullptr) {
            this->load_toybox();
        }

        const char* tag_uid;
        const char* tag_index = "-1";
        for (int i = 0; i < request->params(); i++) {
            const AsyncWebParameter* param = request->getParam(i);

            if (param->name() == "uid") {
                tag_uid = param->value().c_str();
            } else if (param->name() == "index") {
                tag_index = param->value().c_str();
            } else {
                log_warn("[server::toybox] Unknown param %s", param->name().c_str());
            }
        }

        if (tag_uid == nullptr) {
            request->send(400, "text/plain", "Missing Tag UID");
            return;
        }

        ToyTag* tag = this->toybox->getByUID(tag_uid);
        if (tag == nullptr) {
            request->send(404, "text/plain", "Unable to find tag");
            return;
        }

        const TagIndex lastLocation = (*tag).padIndex;
        const TagIndex location = static_cast<TagIndex>(std::stoi(tag_index));
        (*tag).padIndex = location;

        this->_toyboxDirty = true;
        this->toypad->tagChangeEvent(tag, lastLocation);

        request->send(200, "application/json", this->toybox->serialize());
    });

    server->on("/platform", HTTP_POST, [this](AsyncWebServerRequest* request) {
        const char* platform = nullptr;
        for (int i = 0; i < request->params(); i++) {
            const AsyncWebParameter* param = request->getParam(i);

            if (param->name() == "platform") {
                platform = param->value().c_str();
            } else {
                log_warn("[server::platform] Unknown param %s", param->name().c_str());
            }
        }

        if (platform == nullptr) {
            request->send(400, "text/plain", "Missing Platform");
            return;
        }

        const ToypadPlatform platformInt =
            ToypadPlatform(static_cast<uint8_t>(std::stoi(platform)));

        log_dbg("[server::platform] Setting platform to %s",
                platformInt == ToypadPlatform::PS3 ? "PS3" : "X360");
        config.setUChar("platform", static_cast<uint8_t>(platformInt));
        request->send(200, "text/plain", "Success... Restarting...");

        rp2040.reboot();
    });

    ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                      void* arg, uint8_t* data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                log_dbg("[LegoServer] WebSocket client #%u connected", client->id());
                // Push current state immediately on connect so the page
                // doesn't have to wait for the next change
                if (this->toypad) {
                    JsonDocument doc;
                    static constexpr struct {
                        const char* key;
                        PadLocation loc;
                    } ENTRIES[] = {
                        {"left", PadLocation::LEFT},
                        {"center", PadLocation::CENTER},
                        {"right", PadLocation::RIGHT},
                    };
                    for (const auto& e : ENTRIES) {
                        JsonObject obj = doc[e.key].to<JsonObject>();
                        this->toypad->getPad(e.loc)->toJson(obj);
                    }
                    String json;
                    serializeJson(doc, json);
                    client->text(json);
                }
                break;

            case WS_EVT_DISCONNECT:
                log_dbg("[LegoServer] WebSocket client #%u disconnected", client->id());
                break;

            case WS_EVT_DATA: {
                AwsFrameInfo* info = (AwsFrameInfo*)arg;
                if (info->final && info->index == 0 && info->len == len
                    && info->opcode == WS_TEXT) {
                    char msg[len + 1];
                    memcpy(msg, data, len);
                    msg[len] = '\0';

                    if (strncmp(msg, "{\"type\":\"ping\"}", len) == 0) {
                        log_dbg("[LegoServer] Ping from client #%u", client->id());
                        client->text("{\"type\":\"pong\"}");
                    }
                }
                break;
            }

            default:
                break;
        }
    });

    server->addHandler(&ws);

    if (toypad != nullptr) {
        toypad->onPadStateChange([this](const String& serializedPadState) {
            if (ws.count() == 0) {
                return;
            }

            log_dbg("[LegoServer] Broadcasting pad state to %u client(s)", ws.count());
            ws.textAll(serializedPadState);
        });
    }

    return true;
}

bool LegoServer::load_toybox() {
    this->toybox = new Toybox();

    if (!fs_exists("/toybox.json")) {
        return true;
    }

    const char* json = fs_read("/toybox.json");
    if (!json) {
        return false;
    }

    bool result = this->toybox->deserialize(json);
    free((void*)json);

    if (toypad != nullptr) {
        toypad->onTagStateChange([this](const ToyTag* updated) { this->_toyboxDirty = true; });

        // Now that we've loaded the toybox, we can send all of the toybox tag-updates to the
        // toypad.
        // This way the toypad knows who is on what tag index.
        for (size_t i = 0; i < this->toybox->count(); i++) {
            ToyTag* tag = this->toybox->getToy(i);
            if (tag->padIndex != TagIndex::Unplaced || tag->padIndex != TagIndex::INVALID) {
                toypad->tagChangeEvent(tag, TagIndex::Unplaced);
            }
        }
    }

    return result;
}

bool LegoServer::store_toybox() {
    if (this->toybox == nullptr) {
        return false;
    }

    String serialized = this->toybox->serialize();
    const char* content = serialized.c_str();
    bool success = fs_write("/toybox.json", content);
    if (!success) {
        log_warn("[LegoServer::store_toybox] Failed to write toybox cache file");
    }

    if (ws.count() != 0) {
        log_dbg("[LegoServer] Broadcasting toybox state update to %u client(s)", ws.count());

        JsonDocument wsDoc;
        JsonObject wsObj = wsDoc.to<JsonObject>();

        wsObj["type"] = "toybox";
        wsObj["toybox"] = this->toybox->convertToJson();

        String result;
        serializeJson(wsObj, result);

        ws.textAll(result);
    }

    return success;
}