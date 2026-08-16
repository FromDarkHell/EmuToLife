#pragma once

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

// -----------------------------------------------------------------------
//  Interface 3 is an implementation of XSM3 security (class=0xFF / subclass=0xFD / protocol=0x13)
//  Descriptor: 9 (iface) + 6 (XSM3 inline vendor desc) = 15 bytes
//  No data endpoints.
//
//  The Xbox 360 drives the XSM3 authentication handshake entirely through
//  vendor control transfers targeting this interface (bRequest 0x81/0x82/
//  0x83/0x84/0x86/0x87). The actual cryptography is delegated to libxsm3
//  (src/xbox/libxsm3); this class wires TinyUSB's control transfer stages
//  to it and holds the small amount of state that must persist for the
//  lifetime of the USB connection, since the Xbox re-runs the handshake
//  repeatedly (e.g. on every power cycle) without the microcontroller ever
//  detaching.
// -----------------------------------------------------------------------
class X360IfaceXSM3 : public Adafruit_USBD_Interface {
   public:
    void setStringIndex(uint8_t idx) { _strid = idx; }
    uint16_t getInterfaceDescriptor(uint8_t itfnum_deprecated,
                                    uint8_t* buf,
                                    uint16_t bufsize) override;

    // Called from tud_vendor_control_xfer_cb() at CONTROL_STAGE_SETUP.
    // Returns true if this was an XSM3 request and has been handled (either
    // queued via tud_control_xfer(), or ACK'd); false if it isn't one of
    // ours and the caller should keep looking at other request handlers.
    bool handleSetup(uint8_t rhport, tusb_control_request_t const* request);

    // Called from tud_vendor_control_xfer_cb() at CONTROL_STAGE_ACK, once
    // any OUT data phase queued by handleSetup() has landed in our buffers.
    void handleAck(tusb_control_request_t const* request);

    // Resets persisted XSM3 state. Only call this on a genuine physical
    // disconnect (tud_umount_cb) - never on a re-authentication attempt, or
    // the Xbox will stop trusting this device after its next power cycle.
    void reset();

   private:
    // Two-byte state value returned for request 0x86 ("done?" poll).
    // We always finish challenge init/verify synchronously inside
    // handleAck(), so by the time the Xbox polls us the answer is always
    // "complete".
    static constexpr uint16_t STATE_COMPLETE = 0x0002;

    static uint16_t clampLen(uint16_t requested, uint16_t actual) {
        return requested < actual ? requested : actual;
    }

    bool _initialised = false;

    // OUT data landing buffers for the two host->device requests.
    uint8_t _challengeInitBuf[0x22];
    uint8_t _challengeVerifyBuf[0x16];

    // Length of the currently-valid xsm3_challenge_response contents, set
    // once challenge init/verify has actually been processed in handleAck().
    uint16_t _responseLen = 0;
};
