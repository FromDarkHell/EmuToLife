#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <infinity/constants.h>
#include <infinity/figure_crypto.h>
#include <infinity/tag.h>
#include "toy.h"

/**
 * @brief Adds an identifier (id/name) and JSON (de)serialization on top of the library's
 * DisneyInfinity::Figure, and implements the Toy interface so a Toybox can hold this alongside
 * other games' toy types.
 *
 * Also writes the real, AES-encrypted character-ID block (see DisneyInfinity::FigureCrypto) into
 * the figure's game-side block 0 whenever `figureId`/UID are set, so a real game reading this
 * figure via READ/MODEL actually identifies it as the chosen character - not just this firmware's
 * own UI.
 */
class InfinityToyTag : public Toy, public DisneyInfinity::Figure {
   public:
    uint32_t figureId;
    char figureName[48];
    // The frontend's tag-category key (e.g. "characters", "playsets", "abilities",
    // "powerdiscs") - Infinity has no built-in figure/playset/disc type distinction the way LEGO
    // does, so this is purely carried through for the frontend's CDN image lookup and display.
    char figureType[24];
    int16_t padIndex;

    InfinityToyTag() : DisneyInfinity::Figure(), figureId(0), padIndex(-1) {
        figureName[0] = '\0';
        strlcpy(figureType, "characters", sizeof(figureType));
        randomizeUID();
        _writeCharacterIdBlock();
    }

    InfinityToyTag(uint32_t id, const char* name, const char* type = "characters")
        : DisneyInfinity::Figure(), figureId(id), padIndex(-1) {
        strlcpy(figureName, name, sizeof(figureName));
        strlcpy(figureType, (type && type[0] != '\0') ? type : "characters", sizeof(figureType));
        randomizeUID();
        _writeCharacterIdBlock();
    }

    ToyGame toyGame() const override { return ToyGame::DisneyInfinity; }
    uint32_t toyId() const override { return figureId; }
    const char* toyName() const override { return figureName; }
    const char* toyType() const override { return figureType; }

    /**
     * @brief The pad section this figure is currently placed on, or `PadLocation::ALL` if it
     * isn't placed. Backed by `padIndex` for JSON compatibility - any value other than
     * CENTER/LEFT/RIGHT (including the `-1` "unplaced" default) reads back as `ALL`.
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

    // Plain accessors of the raw stored field (mirroring LEGO's ToyTag::rawLocation(), which
    // likewise round-trips its TagIndex field verbatim) - unlike location(), this preserves
    // whatever was actually stored (including the `-1` "never placed" default) rather than
    // normalizing it to `PadLocation::ALL`. That distinction matters to callers like server.cpp
    // that need to tell "never placed" apart from "explicitly set to ALL" generically across games.
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
        strlcpy(figureType, input["type"] | "characters", sizeof(figureType));
        setUIDStr(input["uid"] | "");
        _writeCharacterIdBlock();
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

    /**
     * @brief (Re)writes the AES-encrypted character-ID block for the current figureId/UID into
     * game-side block 0. Must be called any time either changes.
     */
    void _writeCharacterIdBlock() {
        uint8_t uid[DisneyInfinity::FigureCrypto::UID_SIZE];
        getUID(uid);

        uint8_t key[DisneyInfinity::FigureCrypto::KEY_SIZE];
        DisneyInfinity::FigureCrypto::deriveKey(uid, key);

        uint8_t plaintext[DisneyInfinity::FigureCrypto::BLOCK_SIZE];
        DisneyInfinity::FigureCrypto::buildPlaintextBlock(figureId, plaintext);

        uint8_t ciphertext[DisneyInfinity::FigureCrypto::BLOCK_SIZE];
        DisneyInfinity::FigureCrypto::encryptBlock(key, plaintext, ciphertext);

        setBlock(0, ciphertext);
    }
};
