#include "config/config.h"

Config config;

// --- File format ---
// Each line: key=value\n
// e.g.:
//   brightness=128
//   enabled=1
//   name=my-device

static void sanitize(const char* in, char* out, size_t len) {
    // Strip '=' and '\n' from keys/values to keep format safe
    size_t j = 0;
    for (size_t i = 0; in[i] && j < len - 1; i++) {
        if (in[i] != '=' && in[i] != '\n' && in[i] != '\r') {
            out[j++] = in[i];
        }
    }
    out[j] = '\0';
}

// ----------------------------------------------------------------

void Config::init() {
    log_dbg("[Config::init] Mounting LittleFS");
    if (!LittleFS.begin()) {
        log_dbg("[Config::init] Formatting LittleFS (first boot?)");
        LittleFS.format();
        LittleFS.begin();
    }
}

void Config::clear() {
    LittleFS.remove(CONFIG_FILE);
}

// ----------------------------------------------------------------

bool Config::readKey(const char* key, char* out_buf, size_t out_len) const {
    File f = LittleFS.open(CONFIG_FILE, "r");
    if (!f) {
        return false;
    }

    char line[256];
    size_t key_len = strlen(key);

    while (f.available()) {
        int len = f.readBytesUntil('\n', line, sizeof(line) - 1);
        line[len] = '\0';

        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            strncpy(out_buf, line + key_len + 1, out_len - 1);
            out_buf[out_len - 1] = '\0';
            // Strip trailing \r if present
            size_t l = strlen(out_buf);
            if (l > 0 && out_buf[l - 1] == '\r') {
                out_buf[l - 1] = '\0';
            }
            f.close();
            return true;
        }
    }

    f.close();
    return false;
}

bool Config::writeKey(const char* key, const char* value) {
    // Read all existing lines, skipping the old key if present
    File f = LittleFS.open(CONFIG_FILE, "r");

    String updated = "";
    bool found = false;
    size_t key_len = strlen(key);

    if (f) {
        char line[256];
        while (f.available()) {
            int len = f.readBytesUntil('\n', line, sizeof(line) - 1);
            line[len] = '\0';
            if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
                found = true;
                continue;  // Skip old entry
            }
            // Strip trailing \r
            size_t l = strlen(line);
            if (l > 0 && line[l - 1] == '\r') {
                line[l - 1] = '\0';
            }
            if (strlen(line) > 0) {
                updated += String(line) + "\n";
            }
        }
        f.close();
    }

    // Append new key=value
    char safe_key[128], safe_val[128];
    sanitize(key, safe_key, sizeof(safe_key));
    sanitize(value, safe_val, sizeof(safe_val));
    updated += String(safe_key) + "=" + String(safe_val) + "\n";

    // Write back
    File out = LittleFS.open(CONFIG_FILE, "w");
    if (!out) {
        return false;
    }
    out.print(updated);
    out.close();
    return true;
}

// ----------------------------------------------------------------

bool Config::getBool(const char* key, bool default_value) const {
    char buf[8];
    if (!readKey(key, buf, sizeof(buf))) {
        return default_value;
    }
    return atoi(buf) != 0;
}

bool Config::setBool(const char* key, bool value) {
    return writeKey(key, value ? "1" : "0");
}

uint8_t Config::getUChar(const char* key, uint8_t default_value) const {
    char buf[8];
    if (!readKey(key, buf, sizeof(buf))) {
        return default_value;
    }
    return (uint8_t)atoi(buf);
}

bool Config::setUChar(const char* key, uint8_t value) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", value);
    return writeKey(key, buf);
}

uint32_t Config::getUInt(const char* key, uint32_t default_value) const {
    char buf[16];
    if (!readKey(key, buf, sizeof(buf))) {
        return default_value;
    }
    return (uint32_t)strtoul(buf, nullptr, 10);
}

bool Config::setUInt(const char* key, uint32_t value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", value);
    return writeKey(key, buf);
}

const char* Config::getString(const char* key, const char* default_value) const {
    char buf[256];
    if (!readKey(key, buf, sizeof(buf))) {
        char* def = (char*)malloc(strlen(default_value) + 1);
        strcpy(def, default_value);
        return def;
    }
    char* result = (char*)malloc(strlen(buf) + 1);
    strcpy(result, buf);
    return result;
}

bool Config::setString(const char* key, const char* value) {
    return writeKey(key, value);
}