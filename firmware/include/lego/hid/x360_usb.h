#pragma once

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <xbox/x360_usb_stubs.h>
#include <xbox/x360_xsm3.h>

class X360IfacePortal : public Adafruit_USBD_Interface {
   public:
    uint16_t getInterfaceDescriptor(uint8_t itfnum_deprecated,
                                    uint8_t* buf,
                                    uint16_t bufsize) override;
};

class X360PortalUSB {
   public:
    X360PortalUSB();

    // Register all four interfaces with TinyUSBDevice and configure the
    // device descriptor strings.
    bool begin();

    // Send a 32-byte portal data packet (caller has already prepended 0x0B 0x16).
    bool sendReport(const uint8_t* buffer, uint16_t len);

    // Register the callback invoked when the host writes a portal packet to EP OUT.
    using ReceiveCb = void (*)(uint8_t const* buf, uint16_t len);
    void setReceiveCallback(ReceiveCb cb) { _receiveCb = cb; }

    // Called from the tud_vendor_rx_cb() override in x360_usb.cpp.
    void _handleRx(uint8_t const* buf, uint16_t len);

    // Exposed so tud_vendor_control_xfer_cb()/tud_umount_cb() in x360_usb.cpp
    // can route XSM3 control requests straight to the interface that owns
    // that state.
    X360IfaceXSM3& xsm3() { return _ifaceXSM3; }

    static X360PortalUSB* _instance;

   private:
    X360IfacePortal _ifacePortal;
    X360IfaceController _ifaceController;
    X360IfaceExtra _ifaceExtra;
    X360IfaceXSM3 _ifaceXSM3;

    ReceiveCb _receiveCb = nullptr;
};
