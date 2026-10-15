#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <tags/mifare_s20.h>
#include <util/reader.h>

/**
 * @brief JSON (de)serialization of a MIFARE_S20 tag's pages.
 *
 * Pages are stored under a "data" object, keyed by page index. Each value is an array of two
 * uint64s, in little-endian order. This doesn't include pages that haven't been written to yet.
 */
namespace MifareS20Json {

constexpr const char* DATA_KEY = "data";
constexpr uint8_t HALF_PAGE_SIZE = Tags::MIFARE_S20::PAGE_SIZE / 2;

inline void writePages(const Tags::MIFARE_S20& tag, JsonDocument& doc) {
    JsonObject data = doc[DATA_KEY].to<JsonObject>();

    for (uint8_t page = 0; page < Tags::MIFARE_S20::PAGE_COUNT; page++) {
        uint8_t bytes[Tags::MIFARE_S20::PAGE_SIZE];
        if (!tag.get(page, bytes)) {
            continue;
        }

        const uint64_t low = Reader::readUInt64LE(bytes);
        const uint64_t high = Reader::readUInt64LE(bytes, HALF_PAGE_SIZE);
        if (low == 0 && high == 0) {
            continue;
        }

        JsonArray pair = data[String(page)].to<JsonArray>();
        pair.add(low);
        pair.add(high);
    }
}

/**
 * @brief Applies every page in `input["data"]` onto `tag`.
 *
 * @return true if `input` had a "data" object (the tag's buffer is then fully described by it, so
 * any page it omits is zeroed); false if not, leaving `tag` untouched.
 */
inline bool readPages(Tags::MIFARE_S20& tag, const JsonVariantConst input) {
    const JsonObjectConst data = input[DATA_KEY];
    if (data.isNull()) {
        return false;
    }

    const uint8_t zeroPage[Tags::MIFARE_S20::PAGE_SIZE] = {0};
    for (uint8_t page = 0; page < Tags::MIFARE_S20::PAGE_COUNT; page++) {
        tag.set(page, zeroPage);
    }

    for (JsonPairConst entry : data) {
        const long page = strtol(entry.key().c_str(), nullptr, 10);
        const JsonArrayConst pair = entry.value();
        if (page < 0 || page >= Tags::MIFARE_S20::PAGE_COUNT || pair.size() != 2) {
            continue;
        }

        uint8_t bytes[Tags::MIFARE_S20::PAGE_SIZE];
        Reader::writeUInt64LE(bytes, 0, pair[0].as<uint64_t>());
        Reader::writeUInt64LE(bytes, HALF_PAGE_SIZE, pair[1].as<uint64_t>());
        tag.set(static_cast<uint8_t>(page), bytes);
    }

    return true;
}

}  // namespace MifareS20Json
