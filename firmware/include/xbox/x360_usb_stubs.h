#pragma once

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

// -----------------------------------------------------------------------
//  Interface 1 -> Controller channels stub (class=0xFF / subclass=0x5D / protocol=0x03)
//  Descriptor: 9 (iface) + 27 (vendor extra) + 4 × 7 (EPs) = 64 bytes
// -----------------------------------------------------------------------
class X360IfaceController : public Adafruit_USBD_Interface {
   public:
    uint16_t getInterfaceDescriptor(uint8_t itfnum_deprecated,
                                    uint8_t* buf,
                                    uint16_t bufsize) override;
};

// -----------------------------------------------------------------------
//  Interface 2 -> Additional channel stub (class=0xFF / subclass=0x5D / protocol=0x02)
//  Descriptor: 9 (iface) + 9 (vendor extra) + 7 (EP IN) = 25 bytes
// -----------------------------------------------------------------------
class X360IfaceExtra : public Adafruit_USBD_Interface {
   public:
    uint16_t getInterfaceDescriptor(uint8_t itfnum_deprecated,
                                    uint8_t* buf,
                                    uint16_t bufsize) override;
};
