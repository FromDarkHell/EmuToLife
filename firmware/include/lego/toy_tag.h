#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <lego/constants.h>
#include <lego/tag.h>
#include "toy.h"

using ToyTagType = LEGODimensions::ToyTagType;
using TagIndex = LEGODimensions::TagIndex;

/**
 * @brief Adds JSON (de)serialization on top of the library's LEGODimensions::ToyTag, and
 * implements the Toy interface so a Toybox can hold this alongside other games' toy types. JSON is
 * kept local to this firmware rather than the library, since persistence format is an application
 * concern, not a protocol one.
 *
 * Inherits `LEGODimensions::ToyTag` virtually so that `VehicleTag`/`CharacterTag` below can
 * multiply-inherit both this (for JSON) and `LEGODimensions::VehicleTag`/`CharacterTag` (for the
 * NFC buffer layout) without ending up with two separate ToyTag subobjects.
 */
class ToyTag : public Toy, public virtual LEGODimensions::ToyTag {
   public:
    using LEGODimensions::ToyTag::ToyTag;

    /**
     * @brief A string describing the tag's name, used for display/serialization purposes. Kept
     * here rather than on the library's LEGODimensions::ToyTag since persistence format is an
     * application concern, not a protocol one.
     */
    char name[48] = "";

    ToyGame toyGame() const override { return ToyGame::LegoDimensions; }
    uint32_t toyId() const override { return id; }
    const char* toyName() const override { return name; }
    const char* toyType() const override { return typeString(); }

    void getUIDStr(char* out) const override { LEGODimensions::ToyTag::getUIDStr(out); }

    int rawLocation() const override { return static_cast<int>(padIndex); }
    void setRawLocation(int loc) override { padIndex = static_cast<TagIndex>(loc); }

    JsonDocument toJSON() const override {
        JsonDocument doc;
        fillJSON(doc);
        return doc;
    }

    /**
     * @brief Restores all of the game-specific metadata from a JSONVariant
     *
     */
    bool fromJSON(const JsonVariant input) override {
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
        doc["game"] = toyGameString(ToyGame::LegoDimensions);
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
    VehicleTag() = default;

    VehicleTag(uint16_t vehicleId, const char* vehicleName, uint32_t upg23 = 0, uint32_t upg25 = 0)
        : LEGODimensions::VehicleTag(vehicleId, upg23, upg25) {
        strlcpy(name, vehicleName, sizeof(name));
    }

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
    CharacterTag() = default;

    CharacterTag(uint16_t charId, const char* charName) : LEGODimensions::CharacterTag(charId) {
        strlcpy(name, charName, sizeof(name));
    }

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
