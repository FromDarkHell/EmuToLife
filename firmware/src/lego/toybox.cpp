#include "lego/toybox.h"
#include "log/logger.h"

Toybox::Toybox() : toyCount(0) {}

bool Toybox::addToy(const char* name, const int id, const char* type) {
    // Check if we have space for another toy
    if (toyCount >= MAX_TOYS) {
        return false;
    }

    // Validate input parameters
    if (!name || !type || strlen(name) == 0 || strlen(type) == 0) {
        return false;
    }

    if (strcmp(type, "vehicle") == 0) {
        // TODO: Add the upgrades for this (???)
        toys[toyCount] = new VehicleTag(id, name);
    } else if (strcmp(type, "character") == 0) {
        toys[toyCount] = new CharacterTag(id, name);
    } else {
        return false;
    }

    toyCount++;
    return true;
}

bool Toybox::removeToy(const char* uid) {
    int index = -1;

    char t_uid[15];
    for (int i = 0; i < toyCount; i++) {
        toys[i]->getUIDStr(t_uid);
        if (strcmp(t_uid, uid) == 0) {
            index = i;
            break;
        }
    }

    // Check if index is valid
    if (!isValidIndex(index)) {
        return false;
    }

    // Shift all toys after the removed index down by one position
    for (size_t i = index; i < toyCount - 1; i++) {
        toys[i] = toys[i + 1];
    }

    // Clear the last toy and decrement count
    toyCount--;
    return true;
}

ToyTag* Toybox::getToy(size_t index) {
    return toys[index];
}

ToyTag* Toybox::getByUID(const char* uid) {
    int index = -1;

    char t_uid[15];
    for (int i = 0; i < toyCount; i++) {
        toys[i]->getUIDStr(t_uid);
        if (strcmp(t_uid, uid) == 0) {
            index = i;
            break;
        }
    }

    // Check if index is valid
    if (!isValidIndex(index)) {
        return nullptr;
    }

    return toys[index];
}

JsonDocument Toybox::convertToJson() const {
    // Create a JSON document to hold all toys
    JsonDocument doc;
    JsonArray toyArray = doc.to<JsonArray>();

    // Add each toy to the array
    for (size_t i = 0; i < toyCount; i++) {
        JsonObject toy = toyArray.add<JsonObject>();

        JsonDocument serialized = toys[i]->toJSON();
        toy.set(serialized.as<JsonObjectConst>());
    }

    return doc;
}

String Toybox::serialize() const {
    JsonDocument doc = this->convertToJson();

    String result;
    serializeJson(doc, result);

    return result;
}

bool Toybox::deserialize(const char* json) {
    // Check for valid input
    if (!json || strlen(json) == 0) {
        return false;
    }

    // Create temporary document for parsing
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    // Check for parsing errors
    if (error) {
        return false;
    }

    // Check if the JSON has the expected structure
    if (!doc.is<JsonArray>()) {
        return false;
    }

    JsonArray toyArray = doc.as<JsonArray>();

    // Check if we can fit all the toys
    if (toyArray.size() > MAX_TOYS) {
        return false;
    }

    // Clear existing toys
    clear();

    // Load toys from JSON
    for (JsonVariant toy : toyArray) {
        if (toyCount >= MAX_TOYS) {
            break;  // Safety check to prevent overflow
        }

        const char* typeStr = toy["type"] | "";
        ToyTag* tag;
        if (strcmp(typeStr, "vehicle") == 0) {
            tag = new VehicleTag();
        } else if (strcmp(typeStr, "character") == 0) {
            tag = new CharacterTag();
        } else {
            tag = new ToyTag();
        }
        tag->fromJSON(toy);
        toys[toyCount] = tag;

        toyCount++;
    }

    return true;
}

size_t Toybox::count() const {
    return toyCount;
}

void Toybox::clear() {
    // Clear all toy documents
    for (size_t i = 0; i < toyCount; i++) {
        delete toys[i];
        toys[i] = nullptr;
    }
    toyCount = 0;
}

bool Toybox::isValidIndex(int index) const {
    return index >= 0 && (size_t)index < toyCount;
}