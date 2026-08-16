#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <lego/constants.h>
#include <lego/tag.h>

using ToyTagType = LEGODimensions::ToyTagType;
using TagIndex = LEGODimensions::TagIndex;

/**
 * @brief Adds JSON (de)serialization on top of the library's LEGODimensions::ToyTag. JSON is kept
 * local to this firmware rather than the library, since persistence format is an application
 * concern, not a protocol one.
 *
 * Inherits `LEGODimensions::ToyTag` virtually so that `VehicleTag`/`CharacterTag` below can
 * multiply-inherit both this (for JSON) and `LEGODimensions::VehicleTag`/`CharacterTag` (for the
 * NFC buffer layout) without ending up with two separate ToyTag subobjects.
 */
class ToyTag : public virtual LEGODimensions::ToyTag {
   public:
    using LEGODimensions::ToyTag::ToyTag;

    virtual JsonDocument toJSON() const {
        JsonDocument doc;
        fillJSON(doc);
        return doc;
    }

    /**
     * @brief Restores all of the game-specific metadata from a JSONVariant
     *
     */
    virtual bool fromJSON(const JsonVariant input) {
        parseJSON(input);
        return true;
    }

   protected:
    /**
     * @brief Used to fill JSON content for serialization, should be overridden in subclasses to add
     * in extra fields, etc
     *
     * @param doc
     */
    virtual void fillJSON(JsonDocument& doc) const {
        doc["name"] = name;
        doc["id"] = id;

        char uidBuf[15];
        this->getUIDStr(uidBuf);
        doc["uid"] = uidBuf;

        doc["index"] = static_cast<int16_t>(padIndex);
        doc["type"] = typeString();
    }

    /**
     * @brief Does basic parsing of a JSON document to a ToyTag instance
     *
     * @param doc The JSON serialized data to load
     */
    virtual void parseJSON(const JsonVariant doc) {
        id = doc["id"] | 0;
        padIndex = static_cast<TagIndex>(doc["index"] | 0x00);
        strlcpy(name, doc["name"] | "", sizeof(name));

        const char* typeStr = doc["type"] | "";
        if (strcmp(typeStr, "character") == 0) {
            type = ToyTagType::Character;
        } else if (strcmp(typeStr, "vehicle") == 0) {
            type = ToyTagType::Vehicle;
        } else {
            type = ToyTagType::Unknown;
        }

        this->setUID(doc["uid"] | "");
    }
};

/**
 * @brief A LEGODimensions::VehicleTag with JSON (de)serialization added. All of the page-layout
 * handling (commitToBuffer/readFromBuffer/upgrade fields) comes straight from the library.
 */
class VehicleTag : public ToyTag, public LEGODimensions::VehicleTag {
   public:
    using LEGODimensions::VehicleTag::VehicleTag;

   protected:
    void fillJSON(JsonDocument& doc) const override {
        ToyTag::fillJSON(doc);
        doc["vehicleUpgradesP23"] = upgradesP23;
        doc["vehicleUpgradesP25"] = upgradesP25;
    }

    void parseJSON(const JsonVariant doc) override {
        ToyTag::parseJSON(doc);
        upgradesP23 = doc["vehicleUpgradesP23"] | (uint32_t)0;
        upgradesP25 = doc["vehicleUpgradesP25"] | (uint32_t)0;

        // Restore the byte buffer to match the loaded values
        commitToBuffer();
    }
};

/**
 * @brief A LEGODimensions::CharacterTag with JSON (de)serialization added.
 */
class CharacterTag : public ToyTag, public LEGODimensions::CharacterTag {
   public:
    using LEGODimensions::CharacterTag::CharacterTag;

   protected:
    void fillJSON(JsonDocument& doc) const override {
        ToyTag::fillJSON(doc);
        doc["vehicleUpgradesP23"] = 0;
        doc["vehicleUpgradesP25"] = 0;
    }

    void parseJSON(const JsonVariant doc) override {
        ToyTag::parseJSON(doc);
        // Ignore the upgrade fields for characters
    }
};
