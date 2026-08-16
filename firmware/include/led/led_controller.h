#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include "lego/toypad.h"

/**
 * @brief Drives a WS2812B LED strip split into three segments,
 *        one per PlayPad section (Left / Center / Right).
 */
template <uint8_t DATA_PIN>
class PlaypadLEDController {
   public:
    /**
     * @brief Per-section LED count configuration.
     *        LEDs are laid out contiguously: [left][center][right]
     */
    struct Config {
        uint8_t leftCount;
        uint8_t centerCount;
        uint8_t rightCount;

        uint8_t brightness = 50;
    };

    PlaypadLEDController(Toypad* playpad, Config config) : _playpad(playpad), _config(config) {
        _totalLeds = config.leftCount + config.centerCount + config.rightCount;

        // Pre-compute segment start offsets
        _ranges[0] = {0, config.leftCount};                                       // Left
        _ranges[1] = {config.leftCount, config.centerCount};                      // Center
        _ranges[2] = {config.leftCount + config.centerCount, config.rightCount};  // Right
    }

    /**
     * @brief Allocates the LED buffer and registers the FastLED controller.
     *        Call once from setup(), after PlayPad::begin().
     */
    void begin() {
        _leds = new CRGB[_totalLeds];
        FastLED.addLeds<WS2812B, DATA_PIN, GRB>(_leds, _totalLeds);

        fill_solid(_leds, _totalLeds, CRGB::DimGray);

        FastLED.setBrightness(_config.brightness);
        FastLED.show();

        log_dbg("[PlaypadLEDController] Initialized - pin=%d leds=%d (L:%d C:%d R:%d)", DATA_PIN,
                _totalLeds, _config.leftCount, _config.centerCount, _config.rightCount);
    }

    /**
     * @brief Call from the main loop every tick.
     *        Reads the current interpolated colour from each PlaypadPad and
     *        updates the LED segment. FastLED.show() is only called when at
     *        least one segment has changed, avoiding unnecessary bus traffic.
     */
    void update() {
        if (_playpad == nullptr || _leds == nullptr) {
            return;
        }

        static constexpr struct {
            PadLocation loc;
            uint8_t rangeIdx;
        } MAP[] = {
            {PadLocation::LEFT, 0},
            {PadLocation::CENTER, 1},
            {PadLocation::RIGHT, 2},
        };

        bool dirty = false;

        for (const auto& entry : MAP) {
            const PlaypadPad* pad = _playpad->getPad(entry.loc);
            if (pad == nullptr) {
                continue;
            }

            const CRGB next = _toCRGB(pad->color());
            const PadRange& range = _ranges[entry.rangeIdx];

            for (uint8_t i = 0; i < range.count; i++) {
                CRGB& led = _leds[range.start + i];
                if (led != next) {
                    led = next;
                    dirty = true;
                }
            }
        }

        if (dirty) {
            FastLED.show();
        }
    }

    /**
     * @brief Override the brightness at runtime (0-255).
     */
    void setBrightness(uint8_t brightness) {
        _config.brightness = brightness;
        FastLED.setBrightness(brightness);
        FastLED.show();
    }

    /**
     * @brief Immediately blank all LEDs.
     */
    void clear() {
        fill_solid(_leds, _totalLeds, CRGB::Black);
        FastLED.show();
    }

   private:
    struct PadRange {
        uint8_t start;
        uint8_t count;
    };

    Toypad* _playpad = nullptr;
    CRGB* _leds = nullptr;
    Config _config;
    uint8_t _totalLeds = 0;
    PadRange _ranges[3]{};

    static CRGB _toCRGB(PadColor c) { return CRGB(c.r, c.g, c.b); }
};