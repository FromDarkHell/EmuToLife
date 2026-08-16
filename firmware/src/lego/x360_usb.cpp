#include "lego/hid/x360_usb.h"
#include "log/logger.h"

// -----------------------------------------------------------------------
//  Device descriptor patch
//
//  Adafruit TinyUSB defaults to bDeviceClass=0x00. The X360 only initiates
//  XSM3 authentication when it sees bDeviceClass=0xFF.
// -----------------------------------------------------------------------
extern "C" tusb_desc_device_t const* __real_tud_descriptor_device_cb(void);

extern "C" tusb_desc_device_t const* __wrap_tud_descriptor_device_cb(void) {
    static tusb_desc_device_t patched;
    memcpy(&patched, __real_tud_descriptor_device_cb(), sizeof(patched));
    patched.bDeviceClass = 0xFF;
    patched.bDeviceSubClass = 0xFF;
    patched.bDeviceProtocol = 0xFF;
    return &patched;
}

extern "C" uint8_t const* __real_tud_descriptor_bos_cb(void);

extern "C" uint8_t const* __wrap_tud_descriptor_bos_cb(void) {
    return nullptr;
}

extern "C" uint8_t const* tud_descriptor_bos_cb(void) {
    return NULL;
}

// -----------------------------------------------------------------------
//  XInput capability response payloads
//
//  The Xbox 360 host sends GET_REPORT control requests to Interface 1 to
//  query controller capabilities.  The LEGO portal has no inputs or motors,
//  so all capability bytes are zero.
// -----------------------------------------------------------------------

// bmRequestType=0xC0 bRequest=0x01 wValue=0x0000 → device serial (4 bytes)
static const uint8_t xinput_serial[4] = {0x03, 0x10, 0x8E, 0x28};

// bmRequestType=0xC1 bRequest=0x01 wValue=0x0100 → input capabilities (20 bytes)
static const uint8_t xinput_input_caps[20] = {
    0x00, 0x14,                          // rid=0, rsize=20
    0x00, 0x00,                          // buttons
    0x00, 0x00,                          // leftTrigger, rightTrigger
    0x00, 0x00, 0x00, 0x00,              // leftThumbX, leftThumbY
    0x00, 0x00, 0x00, 0x00,              // rightThumbX, rightThumbY
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // reserved + flags
};

// bmRequestType=0xC1 bRequest=0x01 wValue=0x0000 → vibration capabilities (8 bytes)
static const uint8_t xinput_vibration_caps[8] = {
    0x00, 0x08,        // rid=0, rsize=8
    0x00,              // padding
    0x00, 0x00,        // left_motor, right_motor
    0x00, 0x00, 0x00,  // padding
};

// -----------------------------------------------------------------------
//  X360PortalUSB
// -----------------------------------------------------------------------
X360PortalUSB* X360PortalUSB::_instance = nullptr;

X360PortalUSB::X360PortalUSB() {
    _instance = this;
}

bool X360PortalUSB::begin() {
    _ifaceXSM3.setStringIndex(
        TinyUSBDevice.addStringDescriptor("Xbox Security Method 3, Version 1.00, © 2005 Microsoft "
                                          "Corporation. All rights reserved."));

    // Register interfaces in order so bNumInterfaces ends up as 4 and
    // endpoint allocator hands out addresses in the OEM-matching sequence.
    if (!TinyUSBDevice.addInterface(_ifacePortal)) {
        return false;
    }
    if (!TinyUSBDevice.addInterface(_ifaceController)) {
        return false;
    }
    if (!TinyUSBDevice.addInterface(_ifaceExtra)) {
        return false;
    }
    if (!TinyUSBDevice.addInterface(_ifaceXSM3)) {
        return false;
    }

    return true;
}

bool X360PortalUSB::sendReport(const uint8_t* buffer, uint16_t len) {
    if (!tud_vendor_mounted()) {
        return false;
    }
    uint32_t written = tud_vendor_write(buffer, len);
    tud_vendor_write_flush();
    return written == len;
}

void X360PortalUSB::_handleRx(uint8_t const* buf, uint16_t len) {
    if (_receiveCb) {
        _receiveCb(buf, len);
    }
}

uint16_t X360IfacePortal::getInterfaceDescriptor(uint8_t /*itfnum_deprecated*/,
                                                 uint8_t* buf,
                                                 uint16_t bufsize) {
    constexpr uint16_t LEN = 9 + 17 + 7 + 7;
    if (!buf) {
        return LEN;
    }
    if (bufsize < LEN) {
        return 0;
    }

    uint8_t const itfnum = TinyUSBDevice.allocInterface(1);
    uint8_t const epIn = TinyUSBDevice.allocEndpoint(TUSB_DIR_IN);    // → 0x81
    uint8_t const epOut = TinyUSBDevice.allocEndpoint(TUSB_DIR_OUT);  // → 0x01

    uint8_t* p = buf;

    // Interface descriptor
    *p++ = 0x09;
    *p++ = 0x04;
    *p++ = itfnum;
    *p++ = 0x00;
    *p++ = 0x02;  // bNumEndpoints
    *p++ = 0xFF;
    *p++ = 0x5D;
    *p++ = 0x01;
    *p++ = 0x00;

    *p++ = 0x11;
    *p++ = 0x21;
    *p++ = 0x10;
    *p++ = 0x01;
    *p++ = 0x21;
    *p++ = 0x25;
    *p++ = epIn;
    *p++ = 0x14;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x13;
    *p++ = 0x01;
    *p++ = 0x08;
    *p++ = 0x00;
    *p++ = 0x00;

    // EP IN  (Interrupt, 32 B, 4 ms)
    *p++ = 0x07;
    *p++ = 0x05;
    *p++ = epIn;
    *p++ = 0x03;
    *p++ = 0x20;
    *p++ = 0x00;
    *p++ = 0x04;

    // EP OUT (Interrupt, 32 B, 4 ms)
    *p++ = 0x07;
    *p++ = 0x05;
    *p++ = epOut;
    *p++ = 0x03;
    *p++ = 0x20;
    *p++ = 0x00;
    *p++ = 0x04;

    return LEN;
}

extern "C" void tud_vendor_rx_cb(uint8_t itf, uint8_t const* /*buffer*/, uint32_t /*bufsize*/) {
    uint8_t data[CFG_TUD_VENDOR_EPSIZE];
    uint32_t avail = tud_vendor_n_available(itf);
    if (avail > sizeof(data)) {
        avail = sizeof(data);
    }
    uint32_t read = tud_vendor_n_read(itf, data, avail);

    if (itf != 0) {
        // Stub interfaces 1-3: drain without processing.
        return;
    }

    if (X360PortalUSB::_instance) {
        X360PortalUSB::_instance->_handleRx(data, read);
    }
}

extern "C" bool tud_vendor_control_xfer_cb(uint8_t rhport,
                                           uint8_t stage,
                                           tusb_control_request_t const* request) {
    if (stage == CONTROL_STAGE_ACK) {
        if (X360PortalUSB::_instance) {
            X360PortalUSB::_instance->xsm3().handleAck(request);
        }
        return true;
    }

    if (stage != CONTROL_STAGE_SETUP) {
        return true;
    }

    const uint8_t bmRT = request->bmRequestType;
    const uint8_t bReq = request->bRequest;
    const uint16_t wVal = request->wValue;

    // XInput device serial
    if (bmRT == 0xC0 && bReq == 0x01 && wVal == 0x0000) {
        return tud_control_xfer(rhport, request, (void*)xinput_serial, sizeof(xinput_serial));
    }

    // XInput input capabilities
    if (bmRT == 0xC1 && bReq == 0x01 && wVal == 0x0100) {
        return tud_control_xfer(rhport, request, (void*)xinput_input_caps,
                                sizeof(xinput_input_caps));
    }

    // XInput vibration capabilities
    if (bmRT == 0xC1 && bReq == 0x01 && wVal == 0x0000) {
        return tud_control_xfer(rhport, request, (void*)xinput_vibration_caps,
                                sizeof(xinput_vibration_caps));
    }

    // XSM3 authentication requests (interface 3)
    if (X360PortalUSB::_instance && X360PortalUSB::_instance->xsm3().handleSetup(rhport, request)) {
        return true;
    }

    // Generic host-to-device ACK fallback for other vendor OUT requests
    // (e.g. controller LED/rumble config on interface 1) that we don't
    // otherwise implement yet. Not logged - the console sends these
    // frequently (roughly once a second) and logging every one over the
    // slow debug UART adds enough latency to the control transfer path
    // that the console's watchdog can decide the portal went unresponsive.
    if (bmRT == 0x41) {
        return tud_control_status(rhport, request);
    }

    return false;
}