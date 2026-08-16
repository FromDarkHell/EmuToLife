#include "xbox/x360_xsm3.h"
#include "log/logger.h"

extern "C" {
#include <lego/hid/constants.h>
#include "libxsm3/xsm3.h"
}

// -----------------------------------------------------------------------
//  Interface 3 -> XSM3 security stub
//
//  Allocates:  1 interface number, no endpoints.
//  _strid must be set by X360PortalUSB::begin() before addInterface().
//  Total descriptor bytes: 9 + 6 = 15
// -----------------------------------------------------------------------
uint16_t X360IfaceXSM3::getInterfaceDescriptor(uint8_t /*itfnum_deprecated*/,
                                               uint8_t* buf,
                                               uint16_t bufsize) {
    constexpr uint16_t LEN = 9 + 6;
    if (!buf) {
        return LEN;
    }
    if (bufsize < LEN) {
        return 0;
    }

    uint8_t const itfnum = TinyUSBDevice.allocInterface(1);

    uint8_t* p = buf;

    // Interface descriptor (no endpoints)
    *p++ = 0x09;
    *p++ = 0x04;
    *p++ = itfnum;
    *p++ = 0x00;
    *p++ = 0x00;  // bNumEndpoints
    *p++ = 0xFF;
    *p++ = 0xFD;
    *p++ = 0x13;
    *p++ = _strid;  // iInterface → XSM3 string

    // XSM3 inline vendor descriptor (type=0x41) -> verbatim from OEM capture.
    *p++ = 0x06;
    *p++ = 0x41;
    *p++ = 0x00;
    *p++ = 0x01;
    *p++ = 0x01;
    *p++ = 0x03;

    return LEN;
}

// -----------------------------------------------------------------------
//  X360IfaceXSM3 - control-request driven wrapper around libxsm3
// -----------------------------------------------------------------------

bool X360IfaceXSM3::handleSetup(uint8_t rhport, tusb_control_request_t const* request) {
    // Dispatch on bRequest alone (not bmRequestType) - these bRequest codes
    // are unique to XSM3 and TinyUSB already infers IN/OUT direction for
    // tud_control_xfer() from the request itself, so we don't need to gate
    // on the exact bmRequestType byte a given console happens to send.
    const uint8_t bReq = request->bRequest;

    switch (bReq) {
        // Get identification data
        case 0x81: {
            if (!_initialised) {
                uint8_t serial[0x0C];
                for (size_t i = 0; i < sizeof(serial); i++) {
                    serial[i] = rand() & 0xFF;
                }
                xsm3_set_vid_pid(serial, LEGODimensions::X360Identifier.VID,
                                 LEGODimensions::X360Identifier.PID);
                xsm3_initialise_state();
                xsm3_set_identification_data(xsm3_id_data_ms_controller);
                _initialised = true;
                log_dbg("[X360][XSM3] State initialised, sending identification data");
            } else {
                log_dbg("[X360][XSM3] Re-auth requested; reusing existing identification data");
            }

            const uint16_t len = clampLen(request->wLength, sizeof(xsm3_id_data_ms_controller));
            return tud_control_xfer(rhport, request, (void*)xsm3_id_data_ms_controller, len);
        }

        // Challenge init (host -> device)
        case 0x82:
            return tud_control_xfer(rhport, request, _challengeInitBuf, sizeof(_challengeInitBuf));

        // Challenge verify (host -> device)
        case 0x87:
            return tud_control_xfer(rhport, request, _challengeVerifyBuf,
                                    sizeof(_challengeVerifyBuf));

        // No-op ACK - seen from real consoles between challenge rounds.
        case 0x84:
            return tud_control_status(rhport, request);

        // Get challenge response
        case 0x83: {
            log_dbg("[X360][XSM3] Sending challenge response (%d bytes)", _responseLen);
            const uint16_t len = clampLen(request->wLength, _responseLen);
            return tud_control_xfer(rhport, request, xsm3_challenge_response, len);
        }

        // Get auth state ("done?" poll)
        case 0x86: {
            const uint16_t len = clampLen(request->wLength, sizeof(STATE_COMPLETE));
            return tud_control_xfer(rhport, request, (void*)&STATE_COMPLETE, len);
        }

        default:
            return false;
    }
}

void X360IfaceXSM3::handleAck(tusb_control_request_t const* request) {
    switch (request->bRequest) {
        case 0x82:
            xsm3_do_challenge_init(_challengeInitBuf);
            _responseLen = 0x5 + 0x28 + 1;  // header + payload + checksum
            log_dbg("[X360][XSM3] Challenge init processed");
            break;
        case 0x87:
            xsm3_do_challenge_verify(_challengeVerifyBuf);
            _responseLen = 0x5 + 0x10 + 1;  // header + payload + checksum
            log_dbg("[X360][XSM3] Challenge verify processed");
            break;
        default:
            break;
    }
}

void X360IfaceXSM3::reset() {
    _initialised = false;
    _responseLen = 0;
}
