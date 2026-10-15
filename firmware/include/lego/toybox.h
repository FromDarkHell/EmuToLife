#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "infinity/toy_tag.h"
#include "kamenrider/toy_tag.h"
#include "lego/toy_tag.h"

/**
 * @brief A Toybox is used to store and keep track of all of the available Toy instances (from any
 * supported game) that can be placed onto the Playpad.
 *
 * A Toybox instance can additionally be deserialized/serialized to/from JSON for persistence.
 */
class Toybox {
   public:
    Toybox();

    bool addToy(const char* name, const int id, const char* type, const char* game = "dimensions");
    bool removeToy(const char* uid);

    Toy* getToy(size_t index);
    Toy* getByUID(const char* uid);

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

    Toy* toys[MAX_TOYS];  // Array to store toy objects
    size_t toyCount;      // Current number of toys
};
