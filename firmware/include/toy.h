#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Which game a toy's identifiers/tag data belong to.
 */
enum class ToyGame : uint8_t { LegoDimensions, DisneyInfinity };

inline const char* toyGameString(ToyGame game) {
    switch (game) {
        case ToyGame::DisneyInfinity:
            return "infinity";
        default:
            return "dimensions";
    }
}

inline ToyGame toyGameFromString(const char* str) {
    if (str && strcmp(str, "infinity") == 0) {
        return ToyGame::DisneyInfinity;
    }
    return ToyGame::LegoDimensions;
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

    virtual JsonDocument toJSON() const = 0;
    virtual bool fromJSON(const JsonVariant input) = 0;
};
