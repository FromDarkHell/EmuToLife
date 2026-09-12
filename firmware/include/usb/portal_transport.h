#pragma once

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <common/packet.h>
#include <constants.h>
#include <functional>
#include "lego/hid/pad.h"
#include "lego/hid/x360_usb.h"
#include "toy.h"

using ToysToLifeLib::ToypadPlatform;
using ToysToLifeLib::USBIdentifier;

/**
 * @brief Shared USB transport plumbing and LED-pad state for any game's Portal/Toypad emulation:
 * platform selection (PS3 HID vs X360 vendor pipe), device enumeration, raw report
 * send/receive dispatch, and the LEDs
 *
 * Implement this class for individual games, like Toypad or InfinityToypad
 */
class PortalTransport {
   public:
    PortalTransport() = default;
    virtual ~PortalTransport() = default;

    /**
     * @brief Brings up the USB device for the persisted platform, using the subclass's device
     * identifiers/report descriptor. Should be called once from setup().
     */
    void begin();

    /**
     * @brief Ticks pad fade/flash state and fires the pad-state-changed callback if needed, then
     * calls `_updateExtra()` for any protocol-specific per-tick work.
     */
    void update();

    /**
     * @brief Drains any queued outgoing reports for the active instance. Safe to call from a
     * TinyUSB completion callback or the main loop; does nothing if no transport is active.
     */
    static void serviceReportQueue();

    /**
     * @brief Returns the active platform for this portal
     *
     * @return ToypadPlatform
     */
    ToypadPlatform platform() const { return _platform; }

    /**
     * @brief Returns the active game this is presenting the USB vendor/product IDs for.
     */
    ToyGame game() const { return _game; }

    PlaypadPad* getPad(PadLocation loc) {
        if (loc == PadLocation::ALL) {
            return nullptr;
        }

        return &PADS[static_cast<uint8_t>(loc) - 1];
    }

    /**
     * @brief Registers a callback that fires whenever any pad's display state changes (color,
     * fade, flash).
     *
     * @param cb  Called with a pre-serialized JSON string of the full pad state. Invoked from the
     *            main loop (update()), never from a USB callback.
     */
    void onPadStateChange(std::function<void(const String&)> cb) { _padStateCallback = cb; }

    /**
     * @brief Places or removes a toy on this portal. `toy->rawLocation()` must already reflect the
     * new location (see `Toy::setRawLocation`) before this is called; `lastRawLocation` is the
     * toy's location prior to that change, in the same per-game numeric domain.
     *
     * Each concrete portal only understands toys from its own game - callers should have already
     * checked `toy->toyGame() == game()` (e.g. server.cpp's `/toypad` handler does), since a
     * mismatched toy here just gets logged and ignored (and reported as failed).
     *
     * @param toy The toy that was placed or lifted.
     * @param lastRawLocation The toy's `rawLocation()` prior to this change.
     * @return true if the placement was accepted; false if it was rejected (e.g. a game-specific
     *         capacity limit, like Infinity's 3-figures-per-pad-section cap). On failure, the
     *         caller is responsible for rolling `toy`'s location back to `lastRawLocation`, since
     *         the tracking table here was never updated to match.
     */
    virtual bool placeToy(Toy* toy, int lastRawLocation) = 0;

    /**
     * @brief Registers a callback that fires whenever a toy's data is updated (e.g. a WRITE
     * command modifying its tag/figure buffer).
     *
     * @param cb  Called with a non-owning pointer to the modified toy.
     */
    void onTagStateChange(std::function<void(const Toy*)> cb) { _tagStateCallback = cb; }

   protected:
    /**
     * @brief Notifies the registered tag-state callback (if any) that a toy's data changed.
     * Concrete portals call this from their WRITE-equivalent handler.
     */
    void _notifyTagStateChange(const Toy* updatedTag) {
        if (_tagStateCallback) {
            _tagStateCallback(updatedTag);
        }
    }

    /**
     * @brief Sets VID/PID/manufacturer/product/serial/device-version for the current _platform.
     */
    virtual void _configureDevice() = 0;

    /**
     * @brief Returns the HID report descriptor used for the PS3 (HID) transport path.
     */
    virtual const uint8_t* _hidReportDescriptor(uint16_t& len) const = 0;

    /**
     * @brief Called once, after the USB device/endpoints are enumerated, for protocol-specific
     * setup (e.g. seeding a PRNG or crypto key).
     */
    virtual void _onBegin() {}

    /**
     * @brief Called once per update() tick, after pad fade/flash state has been advanced, for any
     * protocol-specific per-tick work (e.g. LEGO's queued tag-event dispatch).
     */
    virtual void _updateExtra() {}

    /**
     * @brief Called with a raw report from the host, already run through `_stripFraming`.
     * Subclasses parse/validate/dispatch their own packet format here.
     */
    virtual void _onReport(uint8_t const* buffer, uint16_t bufsize) = 0;

    /**
     * @brief Writes `packetSize` bytes of a fully-built report into `dest`, applying whatever
     * framing this portal's transport needs. Default is a simple copy
     */
    virtual void _addFraming(uint8_t* dest, const uint8_t* packetData, uint16_t packetSize) const {
        memcpy(dest, packetData, packetSize);
    }

    /**
     * @brief Strips any framing `_addFraming` would have applied from a just-received report,
     * before `_onReport` sees it.
     *
     * @param buffer In/out - advanced past any stripped framing bytes.
     * @param bufsize The original report size.
     * @return uint16_t The report size after stripping.
     */
    virtual uint16_t _stripFraming(uint8_t const*& buffer, uint16_t bufsize) const {
        return bufsize;
    }

    /**
     * @brief Applies a color to one pad, or all three when `loc == PadLocation::ALL`. Shared by
     * every protocol's COL-equivalent handler; marks pad state dirty so `update()` notifies
     * listeners next tick.
     */
    void _setColor(PadLocation loc, PadColor color) {
        if (loc == PadLocation::ALL) {
            for (int i = 0; i < NUM_PADS; i++) {
                PADS[i].setColor(color);
            }
        } else {
            getPad(loc)->setColor(color);
        }

        _padStateDirty = true;
    }

    /**
     * @brief Queues a fully-framed report for transmission and immediately tries to flush the
     * backlog over whichever transport `_platform` selects.
     *
     * Reports are always enqueued rather than sent inline, so ordering is preserved even when this
     * is called from both the USB callback context and the main loop, and a response is never lost
     * just because the IN endpoint is still busy with a previous report - it drains on the next
     * `tud_hid_report_complete_cb` / `update()` instead.
     *
     * @return false only if the queue was full and the oldest pending report had to be dropped.
     */
    bool _sendReport(uint8_t const* buffer, uint16_t bufsize);

    /**
     * @brief Reenumerates the USB device so the host re-reads the (possibly just-changed) device
     * descriptor.
     */
    void _reenumerate();

    ToypadPlatform _platform = ToypadPlatform::PS3;
    ToyGame _game = ToyGame::LegoDimensions;

    Adafruit_USBD_HID _usb_hid;
    X360PortalUSB _x360_usb;

    PlaypadPad PADS[NUM_PADS] = {
        PlaypadPad(PadLocation::LEFT),
        PlaypadPad(PadLocation::CENTER),
        PlaypadPad(PadLocation::RIGHT),
    };

    bool _padStateDirty = false;

   private:
    /**
     * @brief Transmits a single report over the active transport right now. Returns false if the
     * endpoint could not accept it (previous IN transfer still in flight, device not mounted).
     */
    bool _transmitReport(uint8_t const* buffer, uint16_t bufsize);

    /**
     * @brief Sends as many queued reports as the endpoint will currently accept, oldest first.
     * Re-entrancy-safe: a call arriving while another is already draining just returns.
     */
    void _flushReportQueue();

    // 16 pending 32-byte reports is far more headroom than the command/response protocol ever
    // needs in normal operation; hitting the cap means the USB link is wedged.
    static constexpr uint8_t REPORT_QUEUE_CAPACITY = 16;

    struct QueuedReport {
        uint8_t data[Common::TOYPAD_MAX_PACKET_SIZE];
        uint16_t size;
    };

    QueuedReport _reportQueue[REPORT_QUEUE_CAPACITY];
    uint8_t _reportQueueHead = 0;   // next slot to transmit
    uint8_t _reportQueueCount = 0;  // slots currently pending
    bool _flushing = false;

    std::function<void(const String&)> _padStateCallback = nullptr;
    std::function<void(const Toy*)> _tagStateCallback = nullptr;

    void _notifyPadStateChange();

    static uint16_t _getReportCallback(uint8_t report_id,
                                       hid_report_type_t report_type,
                                       uint8_t* buffer,
                                       uint16_t reqlen);
    static void _setReportCallback(uint8_t report_id,
                                   hid_report_type_t report_type,
                                   uint8_t const* buffer,
                                   uint16_t bufsize);

    /**
     * @brief A static pointer to the single active transport instance, used by the static
     * TinyUSB callback functions to reach back into instance state.
     */
    static PortalTransport* _instance;
};
