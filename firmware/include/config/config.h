#pragma once

#include <LittleFS.h>
#include <stdint.h>
#include "log/logger.h"

class Config {
   public:
    void clear();
    void init();

    bool getBool(const char* key, bool default_value) const;
    bool setBool(const char* key, bool value);

    uint8_t getUChar(const char* key, uint8_t default_value) const;
    bool setUChar(const char* key, uint8_t value);

    uint32_t getUInt(const char* key, uint32_t default_value) const;
    bool setUInt(const char* key, uint32_t value);

    // Caller must free() the returned pointer
    const char* getString(const char* key, const char* default_value) const;
    bool setString(const char* key, const char* value);

   private:
    static constexpr const char* CONFIG_FILE = "/config.txt";

    bool writeKey(const char* key, const char* value);
    bool readKey(const char* key, char* out_buf, size_t out_len) const;
};

extern Config config;