#include "usb/portal_transport.h"
#include "log/logger.h"

PortalTransport* PortalTransport::_instance = nullptr;

void PortalTransport::begin() {
    // Only one PortalTransport ever actually owns the USB device at a time (LEGO Dimensions vs
    // Disney Infinity are mutually exclusive per boot), so _instance is claimed here rather than
    // in the constructor - both concrete objects exist as globals regardless of which game is
    // active, but only the one that begin()s should receive host reports.
    _instance = this;

    if (!TinyUSBDevice.isInitialized()) {
        TinyUSBDevice.begin(0);
    }

    _configureDevice();

    if (_platform == ToypadPlatform::X360) {
        _x360_usb.setReceiveCallback([](uint8_t const* buf, uint16_t len) {
            PortalTransport::_setReportCallback(0, (hid_report_type_t)0, buf, len);
        });
        _x360_usb.begin();
    } else {
        uint16_t descLen = 0;
        const uint8_t* desc = _hidReportDescriptor(descLen);

        _usb_hid.enableOutEndpoint(true);
        _usb_hid.setPollInterval(1);
        _usb_hid.setReportDescriptor(desc, descLen);
        _usb_hid.setReportCallback(_getReportCallback, _setReportCallback);
        _usb_hid.begin();
    }

    _reenumerate();

    _onBegin();
}

void PortalTransport::update() {
    _flushReportQueue();

    for (int i = 0; i < NUM_PADS; i++) {
        PADS[i].update();
    }

    _updateExtra();

    if (_padStateDirty) {
        _padStateDirty = false;
        _notifyPadStateChange();
    }

    // _updateExtra() may have queued figure-event packets - get them moving this tick rather than
    // waiting for the next completion callback.
    _flushReportQueue();
}

void PortalTransport::_notifyPadStateChange() {
    if (!_padStateCallback) {
        return;
    }

    JsonDocument doc;

    static constexpr struct {
        const char* key;
        PadLocation loc;
    } ENTRIES[] = {
        {"left", PadLocation::LEFT},
        {"center", PadLocation::CENTER},
        {"right", PadLocation::RIGHT},
    };

    for (const auto& e : ENTRIES) {
        JsonObject obj = doc[e.key].to<JsonObject>();
        getPad(e.loc)->toJson(obj);
    }

    String json;
    serializeJson(doc, json);
    _padStateCallback(json);
}

bool PortalTransport::_sendReport(uint8_t const* buffer, uint16_t bufsize) {
    if (bufsize > Common::TOYPAD_MAX_PACKET_SIZE) {
        bufsize = Common::TOYPAD_MAX_PACKET_SIZE;
    }

    uint32_t save = save_and_disable_interrupts();

    bool dropped = false;
    if (_reportQueueCount == REPORT_QUEUE_CAPACITY) {
        // Backlog is full - the link is almost certainly wedged. Drop the oldest pending report so
        // the freshest protocol state still has a path out.
        _reportQueueHead = (_reportQueueHead + 1) % REPORT_QUEUE_CAPACITY;
        _reportQueueCount--;
        dropped = true;
    }

    uint8_t tail = (_reportQueueHead + _reportQueueCount) % REPORT_QUEUE_CAPACITY;
    memcpy(_reportQueue[tail].data, buffer, bufsize);
    _reportQueue[tail].size = bufsize;
    _reportQueueCount++;

    restore_interrupts(save);

    if (dropped) {
        log_warn("[PortalTransport] Report queue full - dropped oldest pending report");
    }

    _flushReportQueue();
    return !dropped;
}

void PortalTransport::_flushReportQueue() {
    uint32_t save = save_and_disable_interrupts();
    if (_flushing) {
        restore_interrupts(save);
        return;
    }
    _flushing = true;
    restore_interrupts(save);

    while (true) {
        save = save_and_disable_interrupts();
        if (_reportQueueCount == 0) {
            restore_interrupts(save);
            break;
        }
        QueuedReport report = _reportQueue[_reportQueueHead];
        restore_interrupts(save);

        if (!_transmitReport(report.data, report.size)) {
            // Endpoint busy or device not mounted - leave the report queued and retry on the next
            // completion callback / main-loop tick.
            break;
        }

        save = save_and_disable_interrupts();
        _reportQueueHead = (_reportQueueHead + 1) % REPORT_QUEUE_CAPACITY;
        _reportQueueCount--;
        restore_interrupts(save);
    }

    _flushing = false;
}

bool PortalTransport::_transmitReport(uint8_t const* buffer, uint16_t bufsize) {
    return (_platform == ToypadPlatform::X360) ? _x360_usb.sendReport(buffer, bufsize)
                                               : _usb_hid.sendReport(0, buffer, bufsize);
}

void PortalTransport::serviceReportQueue() {
    if (_instance != nullptr) {
        _instance->_flushReportQueue();
    }
}

// TinyUSB calls this the instant an IN report finishes transferring to the host. Draining the
// queue here, not just from loop(), keeps report latency low even if loop() briefly stalls and
// stops a just-built response from being lost when it collides with an in-flight report. The HID
// class declares this weak, so this definition overrides it.
extern "C" void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len) {
    (void)instance;
    (void)report;
    (void)len;
    PortalTransport::serviceReportQueue();
}

void PortalTransport::_reenumerate() {
    if (TinyUSBDevice.mounted()) {
        TinyUSBDevice.detach();
        delay(10);
        TinyUSBDevice.attach();
    }
}

uint16_t PortalTransport::_getReportCallback(uint8_t report_id,
                                             hid_report_type_t report_type,
                                             uint8_t* buffer,
                                             uint16_t reqlen) {
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void PortalTransport::_setReportCallback(uint8_t report_id,
                                         hid_report_type_t report_type,
                                         uint8_t const* buffer,
                                         uint16_t bufsize) {
    (void)report_id;
    (void)report_type;

    if (_instance == nullptr) {
        return;
    }

    if (bufsize < Common::TOYPAD_MAX_PACKET_SIZE) {
        return;
    }

    bufsize = _instance->_stripFraming(buffer, bufsize);
    _instance->_onReport(buffer, bufsize);
}
