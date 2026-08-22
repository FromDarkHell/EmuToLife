#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <infinity/tag.h>
#include "toy.h"

/**
 * @brief Adds an identifier (id/name) and JSON (de)serialization on top of the library's
 * DisneyInfinity::Figure, and implements the Toy interface so a Toybox can hold this alongside
 * other games' toy types.
 *
 * Unlike LEGO Dimensions, Infinity has no small numeric ID baked into the tag protocol - the UID
 * *is* the figure's identity - so `figureId`/`figureName` here are purely a firmware/UI concept for
 * picking a character out of the catalog JSON, not something written into the figure's NFC buffer.
 *
 * Tag-placement / RFID emulation for Infinity figures is not implemented yet; this class only
 * exists to let the toybox track Infinity figures by identifier.
 */
class InfinityToyTag : public Toy, public DisneyInfinity::Figure {
   public:
    uint32_t figureId;
    char figureName[48];
    int16_t padIndex;

    InfinityToyTag() : DisneyInfinity::Figure(), figureId(0), padIndex(-1) {
        figureName[0] = '\0';
        randomizeUID();
    }

    InfinityToyTag(uint32_t id, const char* name)
        : DisneyInfinity::Figure(), figureId(id), padIndex(-1) {
        strlcpy(figureName, name, sizeof(figureName));
        randomizeUID();
    }

    ToyGame toyGame() const override { return ToyGame::DisneyInfinity; }
    uint32_t toyId() const override { return figureId; }
    const char* toyName() const override { return figureName; }
    const char* toyType() const override { return "figure"; }

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
        doc["name"] = figureName;
        doc["id"] = figureId;

        char uidBuf[15];
        getUIDStr(uidBuf);
        doc["uid"] = uidBuf;

        doc["index"] = padIndex;
        doc["type"] = toyType();
        doc["game"] = toyGameString(ToyGame::DisneyInfinity);
        return doc;
    }

    bool fromJSON(const JsonVariant input) override {
        figureId = input["id"] | 0;
        padIndex = input["index"] | -1;
        strlcpy(figureName, input["name"] | "", sizeof(figureName));
        setUIDStr(input["uid"] | "");
        return true;
    }

   private:
    void randomizeUID() {
        uint8_t uid[7];
        for (int i = 0; i < 7; i++) {
            uid[i] = (uint8_t)random(0, 256);
        }
        setUID(uid);
    }
};
