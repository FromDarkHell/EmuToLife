#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "lego/toy_tag.h"

/**
 * @brief A Toybox is used to store and keep track of all of the available ToyTag instances that can
 * be placed onto the Playpad.
 *
 * A Toybox instance can additionally be deserialized/serialized to/from JSON for persistence.
 */
class Toybox {
   public:
    Toybox();

    bool addToy(const char* name, const int id, const char* type);
    bool removeToy(const char* uid);

    ToyTag* getToy(size_t index);
    ToyTag* getByUID(const char* uid);

    JsonDocument convertToJson() const;
    String serialize() const;
    bool deserialize(const char* json);

    size_t count() const;

    // Clear all toys
    void clear();

    // Check if a toy exists at the given index
    bool isValidIndex(int index) const;

   private:
    static const size_t MAX_TOYS = 255;        // Maximum number of toys
    static const size_t JSON_BUFFER_SIZE = 8;  // Buffer size for JSON operations

    ToyTag* toys[MAX_TOYS];  // Array to store toy objects
    size_t toyCount;         // Current number of toys
};
