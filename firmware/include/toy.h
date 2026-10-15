#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <constants.h>

/**
 * @brief Which game a toy's identifiers/tag data belong to.
 */
enum class ToyGame : uint8_t { LegoDimensions, DisneyInfinity, KamenRider };

inline const char* toyGameString(ToyGame game) {
    switch (game) {
        case ToyGame::DisneyInfinity:
            return "infinity";
        case ToyGame::KamenRider:
            return "kamenrider";
        default:
            return "dimensions";
    }
}

inline ToyGame toyGameFromString(const char* str) {
    if (str && strcmp(str, "infinity") == 0) {
        return ToyGame::DisneyInfinity;
    }
    if (str && strcmp(str, "kamenrider") == 0) {
        return ToyGame::KamenRider;
    }
    return ToyGame::LegoDimensions;
}

/**
 * @brief Whether `game`'s portal has a known USB identity for `platform`. Kamen Rider SummonRide
 * only has a PS3 identifier, so it can't present as an Xbox 360 portal.
 */
inline bool toyGameSupportsPlatform(ToyGame game, ToysToLifeLib::ToypadPlatform platform) {
    if (game == ToyGame::KamenRider) {
        return platform == ToysToLifeLib::ToypadPlatform::PS3;
    }
    return true;
}

/**
 * @brief Common interface implemented by every game's toy/tag wrapper (see lego/toy_tag.h,
 * infinity/toy_tag.h), so that Toybox can track toys from multiple games without depending on any
 * single game's tag-buffer layout.
 */
class Toy {
   public:
    virtual ~Toy() = default;

    virtual ToyGame toyGame() const = 0;
    virtual uint32_t toyId() const = 0;
    virtual const char* toyName() const = 0;
    virtual const char* toyType() const = 0;

    virtual void getUIDStr(char* out) const = 0;

    /**
     * @brief The toy's current placement, in whatever format that the game uses;
     * Each game's interface interprets this itself when placing/removing the toy.
     */
    virtual int rawLocation() const = 0;
    virtual void setRawLocation(int loc) = 0;

    virtual JsonDocument toJSON() const = 0;
    virtual bool fromJSON(const JsonVariant input) = 0;
};
