#pragma once

#include <Arduino.h>

class Reader {
   public:
    static uint32_t readUInt32LE(const uint8_t* buf, size_t offset = 0) {
        return (uint32_t)buf[offset] | ((uint32_t)buf[offset + 1] << 8)
               | ((uint32_t)buf[offset + 2] << 16) | ((uint32_t)buf[offset + 3] << 24);
    }

    static uint16_t readUInt16LE(const uint8_t* buf, size_t offset = 0) {
        return (uint16_t)buf[offset] | ((uint16_t)buf[offset + 1] << 8);
    }

    static uint32_t readUInt32BE(const uint8_t* buf, size_t offset = 0) {
        return ((uint32_t)buf[offset] << 24) | ((uint32_t)buf[offset + 1] << 16)
               | ((uint32_t)buf[offset + 2] << 8) | (uint32_t)buf[offset + 3];
    }

    static uint16_t readUInt16BE(const uint8_t* buf, size_t offset = 0) {
        return ((uint16_t)buf[offset] << 8) | (uint16_t)buf[offset + 1];
    }

    static void writeUInt32LE(uint8_t* buf, int offset, uint32_t value) {
        buf[offset] = (uint8_t)(value & 0xFF);
        buf[offset + 1] = (uint8_t)((value >> 8) & 0xFF);
        buf[offset + 2] = (uint8_t)((value >> 16) & 0xFF);
        buf[offset + 3] = (uint8_t)((value >> 24) & 0xFF);
    }

    static void writeUInt32BE(uint8_t* buf, int offset, uint32_t value) {
        buf[offset + 3] = (uint8_t)(value & 0xFF);
        buf[offset + 2] = (uint8_t)((value >> 8) & 0xFF);
        buf[offset + 1] = (uint8_t)((value >> 16) & 0xFF);
        buf[offset + 0] = (uint8_t)((value >> 24) & 0xFF);
    }

    static void writeUInt16LE(uint8_t* buf, int offset, uint16_t value) {
        buf[offset] = (uint8_t)(value & 0xFF);
        buf[offset + 1] = (uint8_t)((value >> 8) & 0xFF);
    }

    static void writeUInt16BE(uint8_t* buf, int offset, uint16_t value) {
        buf[offset + 1] = (uint8_t)(value & 0xFF);
        buf[offset + 0] = (uint8_t)((value >> 8) & 0xFF);
    }
};