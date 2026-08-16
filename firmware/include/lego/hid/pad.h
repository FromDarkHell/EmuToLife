#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "lego/hid/constants.h"

/**
 * @brief A PlaypadPad is a specific location of a playpad. It consists of LEDs, as well as a
 * location. The LEDs can be controlled as fading/flashing or a solid color.
 *
 */
class PlaypadPad {
   public:
    explicit PlaypadPad(PadLocation location)
        : _location(location),
          _color(PadColor::Black()),
          _baseColor(PadColor::Black()),
          _mode(PadDisplayMode::SOLID),
          _fade({}),
          _flash({}) {}

    PadLocation location() const { return _location; }
    PadColor color() const { return _color; }
    PadDisplayMode mode() const { return _mode; }
    PadFadeState fadeState() const { return _fade; }
    PadFlashState flashState() const { return _flash; }

    bool isFading() const { return _mode == PadDisplayMode::FADE; }
    bool isFlashing() const { return _mode == PadDisplayMode::FLASH; }

    /**
     * @brief An update/tick function for updating and keeping track of the current pad flashing
     * state. Useful for changing the pad color
     *
     */
    void update() {
        if (_mode == PadDisplayMode::SOLID) {
            return;
        }

        if (millis() - _lastTickMs < _INTERP_RATE) {
            return;
        }

        _lastTickMs = millis();

        switch (_mode) {
            case PadDisplayMode::FADE: {
                if (_fade.speed == 0) {
                    return;
                }

                uint16_t halfCycle = _fadeTick / _fade.speed;
                uint16_t tickInHalf = _fadeTick % _fade.speed;

                // Finished all half-cycles?
                if (_fade.count != 0 && halfCycle >= _fade.count) {
                    // Even count ends on base, odd count ends on target
                    setColor((_fade.count % 2 == 0) ? _baseColor : _fade.target);
                    return;
                }

                // Even half-cycles go base → target, odd go target → base
                bool ascending = (halfCycle % 2 == 0);
                PadColor& from = ascending ? _baseColor : _fade.target;
                PadColor& to = ascending ? _fade.target : _baseColor;

                float t = (float)tickInHalf / (float)_fade.speed;
                _color = PadColor::lerpColor(from, to, t);

                _fadeTick++;
                break;
            }

            case PadDisplayMode::FLASH: {
                uint16_t cycleLength = _flash.onTime + _flash.offTime;
                if (cycleLength == 0) {
                    return;
                }

                uint16_t tickInCycle = _flashTick % cycleLength;
                uint16_t fullCycles = _flashTick / cycleLength;

                // Finished all flashes?
                if (_flash.count != 0 && fullCycles >= _flash.count) {
                    setColor((_flash.count % 2 == 0) ? _baseColor : _fade.target);
                    return;
                }

                _color = (tickInCycle < _flash.onTime) ? _baseColor : _flash.offColor;
                _flashTick++;
                break;
            }

            default:
                break;
        }
    }

    /**
     * @brief Update the current color state to a fixed color
     *
     * @param color
     */
    void setColor(PadColor color) {
        _baseColor = color;
        _color = color;
        _mode = PadDisplayMode::SOLID;
        _fade = {};
        _flash = {};
        _fadeTick = 0;
        _flashTick = 0;
    }

    /**
     * @brief Changes the pad colors to start fading
     *
     * @param target The color to be fading into
     * @param speed How long to fade
     * @param count How many times it should fade in/out over the `speed` duration
     */
    void setFade(PadColor target, uint8_t speed, uint8_t count) {
        _baseColor = _color;
        _mode = PadDisplayMode::FADE;
        _fade = {target, speed, count};
        _flash = {};
        _fadeTick = 0;
        _lastTickMs = millis();
    }

    /**
     * @brief Changes the pad colors to start flashing
     *
     * @param offColor The RGB color for what color the pad should currently be
     * @param onTime How many ticks (100ms) to show the previous color
     * @param offTime How many ticks (100ms) to show the @see offColor color
     * @param count How many times the pad should flash from onColor (previous) to offColor. An even
     * number keeps the previous color, and an odd number stays as @see offColor
     */

    void setFlash(PadColor offColor, uint8_t onTime, uint8_t offTime, uint8_t count) {
        _baseColor = _color;
        _mode = PadDisplayMode::FLASH;
        _flash = {offColor, onTime, offTime, count};
        _fade = {};
        _flashTick = 0;
        _lastTickMs = millis();
    }

    /**
     * @brief Turns the pad colors off (black colors)
     *
     */
    void setOff() { setColor(PadColor::Black()); }

    /**
     * @brief Serializes this Playpad instance into a JSON object describing its current state
     *
     * @param pad A @see ArduinoJson::V742PB22::JsonObject JSON object to insert the serialization
     * into
     * @return JsonObject
     */
    JsonObject toJson(JsonObject& pad) const {
        pad["location"] = static_cast<uint8_t>(_location);
        pad["mode"] = static_cast<uint8_t>(_mode);

        JsonObject color = pad["color"].to<JsonObject>();
        color["r"] = _baseColor.r;
        color["g"] = _baseColor.g;
        color["b"] = _baseColor.b;

        switch (_mode) {
            case PadDisplayMode::FADE: {
                JsonObject fade = pad["fade"].to<JsonObject>();
                fade["speed"] = _fade.speed;
                fade["count"] = _fade.count;

                JsonObject target = fade["target"].to<JsonObject>();
                target["r"] = _fade.target.r;
                target["g"] = _fade.target.g;
                target["b"] = _fade.target.b;
                break;
            }
            case PadDisplayMode::FLASH: {
                JsonObject flash = pad["flash"].to<JsonObject>();
                flash["onTime"] = _flash.onTime;
                flash["offTime"] = _flash.offTime;
                flash["count"] = _flash.count;

                JsonObject offColor = flash["offColor"].to<JsonObject>();
                offColor["r"] = _flash.offColor.r;
                offColor["g"] = _flash.offColor.g;
                offColor["b"] = _flash.offColor.b;
                break;
            }
            default:
                break;
        }

        return pad;
    }

   private:
    PadLocation _location;

    /**
     * @brief The previous fixed-color state
     *
     */
    PadColor _baseColor;

    /**
     * @brief The currently active color
     *
     */
    PadColor _color;

    PadDisplayMode _mode;
    PadFadeState _fade;
    PadFlashState _flash;

    uint32_t _lastTickMs = 0;
    uint32_t _fadeTick = 0;   // current tick within the full fade sequence
    uint32_t _flashTick = 0;  // current tick within the full flash sequence

    /**
     * @brief The current tick count for the current effect/mode
     * Set to -1 for `COLOR`, and `count` for the other two modes.
     *
     */
    int16_t _effectTickCount = 0;

    /**
     * @brief A rate specifying how long a "tick" is in milliseconds. This value is basically
     * equivalent to: `(25.5 * 1000) / 255`
     *
     */
    uint32_t _INTERP_RATE = 100;
};