#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <kamenrider/constants.h>
#include <kamenrider/tag.h>
#include "mifare_s20_json.h"
#include "toy.h"

/**
 * @brief Adds a display name and JSON (de)serialization on top of the library's
 * KamenRider::ToyTag, and implements the Toy interface so a Toybox can hold this alongside other
 * games' toy types.
 */
class KamenRiderToyTag : public Toy, public KamenRider::ToyTag {
   public:
    char tagName[48];
    // The frontend's tag-category key (e.g. "characters"), carried through for the frontend's CDN
    // image lookup and display.
    char tagType[24];
    int16_t padIndex;

    KamenRiderToyTag() : KamenRider::ToyTag(), padIndex(-1) {
        tagName[0] = '\0';
        strlcpy(tagType, "characters", sizeof(tagType));
    }

    KamenRiderToyTag(uint32_t tagId, const char* name, const char* type = "characters")
        : KamenRider::ToyTag(tagId), padIndex(-1) {
        strlcpy(tagName, name, sizeof(tagName));
        strlcpy(tagType, (type && type[0] != '\0') ? type : "characters", sizeof(tagType));
    }

    ToyGame toyGame() const override { return ToyGame::KamenRider; }
    uint32_t toyId() const override { return id; }
    const char* toyName() const override { return tagName; }
    const char* toyType() const override { return tagType; }

    /**
     * @brief The pad section this tag is placed on, or `PadLocation::ALL` if it isn't placed. Any
     * value other than CENTER/LEFT/RIGHT (including the `-1` "unplaced" default) reads back as
     * `ALL`.
     */
    PadLocation location() const {
        switch (padIndex) {
            case static_cast<int16_t>(PadLocation::CENTER):
            case static_cast<int16_t>(PadLocation::LEFT):
            case static_cast<int16_t>(PadLocation::RIGHT):
                return static_cast<PadLocation>(padIndex);
            default:
                return PadLocation::ALL;
        }
    }

    void setLocation(PadLocation loc) { padIndex = static_cast<int16_t>(loc); }

    int rawLocation() const override { return padIndex; }
    void setRawLocation(int loc) override { padIndex = static_cast<int16_t>(loc); }

    void getUIDStr(char* out) const override {
        uint8_t uid[7];
        getUID(uid);
        for (int i = 0; i < 7; i++) {
            sprintf(out + i * 2, "%02X", uid[i]);
        }
        out[14] = '\0';
    }

    void setUIDStr(const char* uidStr) {
        uint8_t uid[7] = {0};
        for (int i = 0; i < 7 && strlen(uidStr) >= (size_t)(i * 2 + 2); i++) {
            char byteStr[3] = {uidStr[i * 2], uidStr[i * 2 + 1], '\0'};
            uid[i] = (uint8_t)strtol(byteStr, nullptr, 16);
        }
        setUID(uid);
    }

    JsonDocument toJSON() const override {
        JsonDocument doc;
        doc["name"] = tagName;
        doc["id"] = id;

        char uidBuf[15];
        getUIDStr(uidBuf);
        doc["uid"] = uidBuf;

        doc["index"] = padIndex;
        doc["type"] = toyType();
        doc["game"] = toyGameString(ToyGame::KamenRider);
        MifareS20Json::writePages(*this, doc);
        return doc;
    }

    bool fromJSON(const JsonVariant input) override {
        id = input["id"] | 0;
        padIndex = input["index"] | -1;
        strlcpy(tagName, input["name"] | "", sizeof(tagName));
        strlcpy(tagType, input["type"] | "characters", sizeof(tagType));
        setUIDStr(input["uid"] | "");
        if (!MifareS20Json::readPages(*this, input)) {
            commitToBuffer();
        }
        return true;
    }
};
