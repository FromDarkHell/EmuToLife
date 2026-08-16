#include "xbox/x360_usb_stubs.h"
#include <Arduino.h>
#include "log/logger.h"

uint16_t X360IfaceController::getInterfaceDescriptor(uint8_t /*itfnum_deprecated*/,
                                                     uint8_t* buf,
                                                     uint16_t bufsize) {
    constexpr uint16_t LEN = 9 + 27 + 7 + 7 + 7 + 7;
    if (!buf) {
        return LEN;
    }
    if (bufsize < LEN) {
        return 0;
    }

    uint8_t const itfnum = TinyUSBDevice.allocInterface(1);
    uint8_t const ep82 = TinyUSBDevice.allocEndpoint(TUSB_DIR_IN);   // -> 0x82
    uint8_t const ep02 = TinyUSBDevice.allocEndpoint(TUSB_DIR_OUT);  // -> 0x02
    uint8_t const ep83 = TinyUSBDevice.allocEndpoint(TUSB_DIR_IN);   // -> 0x83
    uint8_t const ep03 = TinyUSBDevice.allocEndpoint(TUSB_DIR_OUT);  // -> 0x03

    uint8_t* p = buf;

    // Interface descriptor
    *p++ = 0x09;
    *p++ = 0x04;
    *p++ = itfnum;
    *p++ = 0x00;
    *p++ = 0x04;  // bNumEndpoints
    *p++ = 0xFF;
    *p++ = 0x5D;
    *p++ = 0x03;
    *p++ = 0x00;

    *p++ = 0x1B;
    *p++ = 0x21;
    *p++ = 0x00;
    *p++ = 0x01;
    *p++ = 0x21;
    *p++ = 0x01;
    *p++ = ep82;  // byte 6
    *p++ = 0x20;
    *p++ = 0x01;
    *p++ = ep02;  // byte 9
    *p++ = 0x20;
    *p++ = 0x16;
    *p++ = ep83;  // byte 12
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x16;
    *p++ = ep03;  // byte 20
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;

    // EP IN1  (Interrupt, 32 B,  2 ms)
    *p++ = 0x07;
    *p++ = 0x05;
    *p++ = ep82;
    *p++ = 0x03;
    *p++ = 0x20;
    *p++ = 0x00;
    *p++ = 0x02;
    // EP OUT1 (Interrupt, 32 B,  4 ms)
    *p++ = 0x07;
    *p++ = 0x05;
    *p++ = ep02;
    *p++ = 0x03;
    *p++ = 0x20;
    *p++ = 0x00;
    *p++ = 0x04;
    // EP IN2  (Interrupt, 32 B, 32 ms)
    *p++ = 0x07;
    *p++ = 0x05;
    *p++ = ep83;
    *p++ = 0x03;
    *p++ = 0x20;
    *p++ = 0x00;
    *p++ = 0x20;
    // EP OUT2 (Interrupt, 32 B, 16 ms)
    *p++ = 0x07;
    *p++ = 0x05;
    *p++ = ep03;
    *p++ = 0x03;
    *p++ = 0x20;
    *p++ = 0x00;
    *p++ = 0x10;

    return LEN;
}

uint16_t X360IfaceExtra::getInterfaceDescriptor(uint8_t /*itfnum_deprecated*/,
                                                uint8_t* buf,
                                                uint16_t bufsize) {
    constexpr uint16_t LEN = 9 + 9 + 7;
    if (!buf) {
        return LEN;
    }
    if (bufsize < LEN) {
        return 0;
    }

    uint8_t const itfnum = TinyUSBDevice.allocInterface(1);
    uint8_t const ep84 = TinyUSBDevice.allocEndpoint(TUSB_DIR_IN);  // -> 0x84

    uint8_t* p = buf;

    // Interface descriptor
    *p++ = 0x09;
    *p++ = 0x04;
    *p++ = itfnum;
    *p++ = 0x00;
    *p++ = 0x01;  // bNumEndpoints
    *p++ = 0xFF;
    *p++ = 0x5D;
    *p++ = 0x02;
    *p++ = 0x00;

    *p++ = 0x09;
    *p++ = 0x21;
    *p++ = 0x00;
    *p++ = 0x01;
    *p++ = 0x21;
    *p++ = 0x22;
    *p++ = ep84;
    *p++ = 0x07;
    *p++ = 0x00;

    *p++ = 0x07;
    *p++ = 0x05;
    *p++ = ep84;
    *p++ = 0x03;
    *p++ = 0x20;
    *p++ = 0x00;
    *p++ = 0x10;

    return LEN;
}
