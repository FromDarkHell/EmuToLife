#pragma once

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <crypto/infinity_prng.h>
#include <infinity/packet.h>
#include <infinity/packets.h>
#include <functional>
#include <unordered_map>
#include "infinity/toy_tag.h"
#include "lego/hid/reports.h"
#include "lego/util/event_queue.h"
#include "log/logger.h"
#include "usb/portal_transport.h"

/**
 * @brief Handles the Disney Infinity USB handshake (WAKE -> SEED -> CHALLENGE), pad coloring/
 * fading/flashing, and figure placement (GETTOYS/MODEL/READ/WRITE), proving the device enumerates
 * and authenticates as a Disney Infinity Base and tracks figures the same way real hardware does.
 *
 * Unlike `Toypad` (LEGO Dimensions), there's no sub-position concept within a pad section - a
 * figure's location *is* one of the three `PadLocation` values directly, so the placement state
 * machine (`figureChangeEvent`) is simpler than `Toypad::tagChangeEvent`'s cross-section logic.
 *
 * Framing: this class doesn't override `_addFraming`/`_stripFraming`, so it inherits the base's
 * plain-copy/no-op defaults - unlike LEGO Dimensions, no verified Infinity-specific X360 vendor
 * pipe framing convention is known yet, so X360 reports are currently sent/received unframed.
 */
class InfinityToypad : public PortalTransport {
   public:
    InfinityToypad() {
        _game = ToyGame::DisneyInfinity;
        _registerHandlers();
    }

    /**
     * @brief Reads the persisted platform choice, then initializes the USB interface. Should be
     * called once from setup().
     */
    void begin();

    inline void sendPacket(DisneyInfinity::BasePacket& packet) {
        log_dbg("[InfinityToypad] Sending packet data: %s", packet.toHexString());

        uint8_t buffer[DisneyInfinity::PLAYPAD_MAX_PACKET_SIZE]{};
        _addFraming(buffer, packet.data, DisneyInfinity::PLAYPAD_MAX_PACKET_SIZE);
        PortalTransport::_sendReport(buffer, DisneyInfinity::PLAYPAD_MAX_PACKET_SIZE);
    }

    /**
     * @brief Fires the appropriate HID figure-event toward the game. If `tag->location()` is
     * `PadLocation::ALL` the figure is treated as removed; any other valid pad location fires a
     * placement event.
     *
     * @param tag  The figure that was placed or lifted. `location()` must already reflect the new
     *             location before this is called.
     * @param lastLocation The figure's location prior to this change (`PadLocation::ALL` if it
     *             wasn't previously placed).
     * @return true if the change was accepted; false if it was rejected (e.g. the target pad
     *             section already has `MAX_FIGURES_PER_LOCATION` figures stacked on it) - the
     *             caller should roll `tag`'s location back to `lastLocation` in that case.
     */
    bool figureChangeEvent(InfinityToyTag* tag, PadLocation lastLocation);

   private:
    /**
     * @brief Casts `toy` to `InfinityToyTag*` (bailing with a warning if it's not actually an
     * Infinity figure) and forwards to `figureChangeEvent`, translating the generic
     * `Toy::rawLocation()` int domain to/from `PadLocation`.
     */
    bool placeToy(Toy* toy, int lastRawLocation) override;

    void _configureDevice() override;

    const uint8_t* _hidReportDescriptor(uint16_t& len) const override {
        len = sizeof(desc_hid_report_ps3);
        return desc_hid_report_ps3;
    }

    void _onBegin() override;
    void _onReport(uint8_t const* buffer, uint16_t bufsize) override;

    /**
     * @brief Xbox 360 typically throws in a few bytes at the start, depending on the pad type.
     * So this shifts everything over by 2 bytes if it is Xbox 360
     *
     */
    void _addFraming(uint8_t* dest, const uint8_t* packetData, uint16_t packetSize) const override {
        if (_platform == ToypadPlatform::X360) {
            dest[0] = 0x0B;
            dest[1] = 0x15;
            memcpy(dest + 2, packetData, packetSize - 2);
        } else {
            memcpy(dest, packetData, packetSize);
        }
    }

    uint16_t _stripFraming(uint8_t const*& buffer, uint16_t bufsize) const override {
        if (_platform == ToypadPlatform::X360 && bufsize >= 2 && buffer[0] == 0x0B
            && buffer[1] == 0x15) {
            buffer += 2;
            bufsize -= 2;
        }

        return bufsize;
    }

    void _handleCommandPacket(const DisneyInfinity::CommandPacket& packet);

    using CommandHandler = std::function<void(const DisneyInfinity::CommandPacket&)>;
    std::unordered_map<uint8_t, CommandHandler> _commandHandlers;

    void _registerHandlers();
    void _handleWake(const DisneyInfinity::CommandPacket& packet);
    void _handleSeed(const DisneyInfinity::CommandPacket& packet);
    void _handleChallenge(const DisneyInfinity::CommandPacket& packet);
    void _handleCol(const DisneyInfinity::CommandPacket& packet);
    void _handleFade(const DisneyInfinity::CommandPacket& packet);
    void _handleFadeAll(const DisneyInfinity::CommandPacket& packet);
    void _handleFlash(const DisneyInfinity::CommandPacket& packet);
    void _handleFlashAll(const DisneyInfinity::CommandPacket& packet);

    /**
     * @brief UNK95 (0x95) is a session-lifecycle command the games send at startup and shutdown
     * with a fixed literal payload (see `DisneyInfinity::Unk95Packet`). Its purpose is
     * unconfirmed; this logs the payload and answers with a blank acknowledgement.
     */
    void _handleUnk95(const DisneyInfinity::CommandPacket& packet);

    /**
     * @brief STATUS's purpose is unconfirmed (per the library's GatewayCommand doc comment) - real
     * hardware just answers it with the same blank acknowledgement as the color/fade/flash
     * commands, so that's all this does for now.
     */
    void _handleStatus(const DisneyInfinity::CommandPacket& packet);

    void _handleGetToys(const DisneyInfinity::CommandPacket& packet);
    void _handleRead(const DisneyInfinity::CommandPacket& packet);
    void _handleWrite(const DisneyInfinity::CommandPacket& packet);
    void _handleModel(const DisneyInfinity::CommandPacket& packet);

    /**
     * @brief Drains one queued figure-event packet per tick (see `_eventQueue`).
     */
    void _updateExtra() override;

    InfinityPRNG _prng;

    /**
     * @brief A figure currently placed on the pad, tracked the same way `Toypad::PlacedToken`
     * tracks LEGO tags.
     */
    struct PlacedFigure {
        uint8_t orderAdded;    // the insertion-order value sent in GETTOYS/events (0-15)
        PadLocation location;  // which of the 3 pad sections
        InfinityToyTag* tag;   // non-owning pointer
    };

    // GetToysPacket::buildResponse supports at most 9 figures per the library's fixed payload size.
    static constexpr uint8_t MAX_FIGURES = 9;

    // Real hardware only allows a character tag plus up to 2 ability/power discs stacked at the
    // same pad section - so at most 3 figures can share a single PadLocation at once.
    static constexpr uint8_t MAX_FIGURES_PER_LOCATION = 3;

    PlacedFigure _placedFigures[MAX_FIGURES]{};
    uint8_t _placedCount = 0x00;

    uint8_t _countAtLocation(PadLocation loc) const {
        uint8_t count = 0;
        for (size_t i = 0; i < _placedCount; i++) {
            if (_placedFigures[i].location == loc) {
                count++;
            }
        }
        return count;
    }

    // orderAdded is a 4-bit field (see FigureEntry::toByte), so only bits 0-15 are ever used.
    uint16_t _orderMask = 0x0000;  // bit N set -> orderAdded N currently in use

    uint8_t _allocOrder() {
        for (uint8_t i = 0; i < 16; i++) {
            if (!(_orderMask & (1u << i))) {
                _orderMask |= (1u << i);
                return i;
            }
        }
        return 0xFF;  // table full
    }

    void _freeOrder(uint8_t order) { _orderMask &= ~(1u << order); }

    PlacedFigure* _findFigure(const InfinityToyTag* tag) {
        for (size_t i = 0; i < _placedCount; i++) {
            if (_placedFigures[i].tag == tag) {
                return &_placedFigures[i];
            }
        }

        return nullptr;
    }

    PlacedFigure* _findByOrder(uint8_t orderAdded) {
        for (size_t i = 0; i < _placedCount; i++) {
            if (_placedFigures[i].orderAdded == orderAdded) {
                return &_placedFigures[i];
            }
        }

        return nullptr;
    }

    EventQueue<DisneyInfinity::EventPacket> _eventQueue;
};
