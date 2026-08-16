#include <Arduino.h>

// clang-format off

// PS3: single vendor interface, minimal descriptor matching OEM wDescriptorLength.
static const uint8_t desc_hid_report_ps3[] = {
    0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,        // Usage (0x01)
    0xA1, 0x01,        // Collection (Application)
    0x19, 0x01,        //   Usage Minimum (1)
    0x29, 0x20,        //   Usage Maximum (32)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x20,        //   Report Count (32)
    0x81, 0x00,        //   Input  (Array)
    0x19, 0x01,        //   Usage Minimum (1)
    0x29, 0x20,        //   Usage Maximum (32)
    0x91, 0x00,        //   Output (Array)
    0xC0               // End Collection
};
static_assert(sizeof(desc_hid_report_ps3) == 29, "PS3 HID descriptor must be 29 bytes");

// clang-format on