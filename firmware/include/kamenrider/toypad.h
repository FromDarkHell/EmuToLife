#pragma once

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <kamenrider/packet.h>
#include <kamenrider/packets.h>
#include <functional>
#include <unordered_map>
#include "kamenrider/toy_tag.h"
#include "lego/hid/reports.h"
#include "lego/util/event_queue.h"
#include "log/logger.h"
#include "usb/portal_transport.h"

/**
 * @brief Emulates the Kamen Rider SummonRide "Ride Gate" portal: USB enumeration, command
 * dispatch, and tracking which tags are currently placed.
 *
 * The Ride Gate only has a PS3 USB identity. The web UI and `/game` refuse to switch to this game
 * while the platform is Xbox 360, and `begin()` forces PS3 as a fallback if that ever slips through.
 */
class KamenRiderToypad : public PortalTransport {
   public:
    KamenRiderToypad() {
        _game = ToyGame::KamenRider;
        _registerHandlers();
    }

    /**
     * @brief Initializes the USB interface on the PS3 transport (the only one the Ride Gate has).
     * Should be called once from setup().
     */
    void begin();

    inline void sendPacket(KamenRider::BasePacket& packet) {
        log_dbg("[KamenRiderToypad] Sending packet data: %s", packet.toHexString());

        uint8_t buffer[KamenRider::PLAYPAD_MAX_PACKET_SIZE]{};
        _addFraming(buffer, packet.data, KamenRider::PLAYPAD_MAX_PACKET_SIZE);
        PortalTransport::_sendReport(buffer, KamenRider::PLAYPAD_MAX_PACKET_SIZE);
    }

    /**
     * @brief Updates the placed-tag tracking table for `tag`. If `tag->location()` is
     * `PadLocation::ALL` the tag is treated as removed; any other pad location is a placement.
     *
     * @param tag  The tag that was placed or lifted. `location()` must already reflect the new
     *             location before this is called.
     * @param lastLocation The tag's location prior to this change (`PadLocation::ALL` if it wasn't
     *             previously placed).
     * @return true if the change was accepted; false if it was rejected (e.g. the tracking table
     *             is full) - the caller should roll `tag`'s location back to `lastLocation`.
     */
    bool tagChangeEvent(KamenRiderToyTag* tag, PadLocation lastLocation);

   private:
    bool placeToy(Toy* toy, int lastRawLocation) override;

    void _configureDevice() override;

    const uint8_t* _hidReportDescriptor(uint16_t& len) const override {
        len = sizeof(desc_hid_report_ps3);
        return desc_hid_report_ps3;
    }

    void _onReport(uint8_t const* buffer, uint16_t bufsize) override;

    /**
     * @brief Drains one queued event packet per tick (see `_eventQueue`).
     */
    void _updateExtra() override;

    void _handleCommandPacket(const KamenRider::CommandPacket& packet);

    using CommandHandler = std::function<void(const KamenRider::CommandPacket&)>;
    std::unordered_map<uint8_t, CommandHandler> _commandHandlers;

    void _registerHandlers();
    void _handleWake(const KamenRider::CommandPacket& packet);
    void _handleCol(const KamenRider::CommandPacket& packet);
    void _handleFlash(const KamenRider::CommandPacket& packet);
    void _handleTagList(const KamenRider::CommandPacket& packet);
    void _handleRead(const KamenRider::CommandPacket& packet);
    void _handleWrite(const KamenRider::CommandPacket& packet);

    /**
     * @brief Set by the first WAKE. Matching RPCS3, placed/removed events are only sent after
     * this; tags placed before it are still reported through TGLST.
     */
    bool _awake = false;

    /**
     * @brief Queues a placed/removed event for `tag`, if the gate has been woken.
     */
    void _queueTagEvent(const KamenRiderToyTag* tag,
                        KamenRider::TagEventPacket::TagMovementDirection direction);

    /**
     * @brief A tag currently placed on the Ride Gate.
     */
    struct PlacedTag {
        PadLocation location;
        KamenRiderToyTag* tag;  // non-owning pointer
    };

    // Taken from RPCS3's Ride Gate emulation, which tracks up to 7 tags; not verified against real
    // hardware.
    static constexpr uint8_t MAX_TAGS = 7;

    PlacedTag _placedTags[MAX_TAGS]{};
    uint8_t _placedCount = 0x00;

    PlacedTag* _findTag(const KamenRiderToyTag* tag) {
        for (size_t i = 0; i < _placedCount; i++) {
            if (_placedTags[i].tag == tag) {
                return &_placedTags[i];
            }
        }

        return nullptr;
    }

    PlacedTag* _findTagByUID(const uint8_t uid[KamenRider::TAG_UID_SIZE]) {
        uint8_t placedUid[KamenRider::TAG_UID_SIZE];
        for (size_t i = 0; i < _placedCount; i++) {
            _placedTags[i].tag->getUID(placedUid);
            if (memcmp(placedUid, uid, KamenRider::TAG_UID_SIZE) == 0) {
                return &_placedTags[i];
            }
        }

        return nullptr;
    }

    EventQueue<KamenRider::EventPacket> _eventQueue;
};
