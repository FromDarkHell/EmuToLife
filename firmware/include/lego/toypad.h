#pragma once

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <crypto/burtle.h>
#include <crypto/tea.h>
#include <lego/packet.h>
#include <lego/packets.h>
#include <functional>
#include "lego/hid/pad.h"
#include "lego/hid/reports.h"
#include "lego/hid/x360_usb.h"
#include "lego/toy_tag.h"
#include "lego/util/event_queue.h"
#include "log/logger.h"
#include "usb/portal_transport.h"

class Toypad : public PortalTransport {
   public:
    Toypad();

    /**
     * @brief Reads the persisted platform/game choice, then initializes the PlayPad device and
     * configures the USB HID interface. This should be called in the setup() function of the main
     * program. After calling this function, the device should be ready to send and receive HID
     * reports with the host.
     *
     */
    void begin();

    inline void sendPacket(LEGODimensions::BasePacket& packet) {
        log_dbg("[Playpad] Sending packet data: %s", packet.toHexString());

        uint8_t buffer[LEGODimensions::PLAYPAD_MAX_PACKET_SIZE]{};
        _addFraming(buffer, packet.data, LEGODimensions::PLAYPAD_MAX_PACKET_SIZE);
        PortalTransport::_sendReport(buffer, LEGODimensions::PLAYPAD_MAX_PACKET_SIZE);
    }

    /**
     * @brief Fires the appropriate HID tag-event toward the game.
     *        If padIndex == TagIndex::Unplaced the tag is treated as
     *        removed; any other valid pad location fires a placement event.
     *
     * @param tag  The tag that was placed or lifted. padIndex must already
     *             reflect the new location before this is called.
     */
    void tagChangeEvent(ToyTag* placedTag, TagIndex lastLocation);

   private:
    /**
     * @brief Casts `toy` to `ToyTag*` (bailing with a warning if it's not actually a LEGO tag) and
     * forwards to `tagChangeEvent`, translating the generic `Toy::rawLocation()` int domain to/from
     * `TagIndex`.
     */
    bool placeToy(Toy* toy, int lastRawLocation) override;

    /**
     * @brief Configures the USB HID device with the correct vendor ID, product ID, and other device
     * properties.
     *
     */
    void _configureDevice() override;

    /**
     * @brief Returns the (game-agnostic) vendor-passthrough HID report descriptor used for the PS3
     * transport path.
     *
     */
    const uint8_t* _hidReportDescriptor(uint16_t& len) const override {
        len = sizeof(desc_hid_report_ps3);
        return desc_hid_report_ps3;
    }

    /**
     * @brief Initializes LEGO-specific session state (PRNG/crypto key) once the USB device is up.
     *
     */
    void _onBegin() override;

    /**
     * @brief Drains one queued tag-event packet per tick (see `_eventQueue`).
     *
     */
    void _updateExtra() override;

    /**
     * @brief Parses and dispatches a raw report from the host as a LEGO Dimensions packet.
     *
     * @param buffer
     * @param bufsize
     */
    void _onReport(uint8_t const* buffer, uint16_t bufsize) override;

    /**
     * @brief Xbox 360 typically throws in a few bytes at the start, depending on the pad type.
     * So this shifts everything over by 2 bytes if it is Xbox 360
     *
     */
    void _addFraming(uint8_t* dest, const uint8_t* packetData, uint16_t packetSize) const override {
        if (_platform == ToypadPlatform::X360) {
            dest[0] = 0x0B;
            dest[1] = 0x16;
            memcpy(dest + 2, packetData, packetSize - 2);
        } else {
            memcpy(dest, packetData, packetSize);
        }
    }

    uint16_t _stripFraming(uint8_t const*& buffer, uint16_t bufsize) const override {
        if (_platform == ToypadPlatform::X360 && bufsize >= 2 && buffer[0] == 0x0B
            && buffer[1] == 0x16) {
            buffer += 2;
            bufsize -= 2;
        }

        return bufsize;
    }

    /**
     * @brief Handles any command packet, and routes it to the specific command handler function
     *
     * @param packet
     */
    void _handleCommandPacket(const LEGODimensions::CommandPacket& packet);

    using CommandHandler = std::function<void(const LEGODimensions::CommandPacket&)>;
    std::unordered_map<uint8_t, CommandHandler> _commandHandlers;

    /**
     * @brief Registers and initializes all of the command handlers for various commands
     *
     */
    void _registerHandlers();
    void _handleWake(const LEGODimensions::CommandPacket& packet);
    void _handleSeed(const LEGODimensions::CommandPacket& packet);
    void _handleChallenge(const LEGODimensions::CommandPacket& packet);
    void _handleCol(const LEGODimensions::CommandPacket& packet);
    void _handleColAll(const LEGODimensions::CommandPacket& packet);
    void _handleFlash(const LEGODimensions::CommandPacket& packet);
    void _handleFlashAll(const LEGODimensions::CommandPacket& packet);
    void _handleFade(const LEGODimensions::CommandPacket& packet);
    void _handleFadeAll(const LEGODimensions::CommandPacket& packet);

    void _handleGetColor(const LEGODimensions::CommandPacket& packet);

    void _handleTagList(const LEGODimensions::CommandPacket& packet);
    void _handleRead(const LEGODimensions::CommandPacket& packet);
    void _handleModel(const LEGODimensions::CommandPacket& packet);

    void _handleWrite(const LEGODimensions::CommandPacket& packet);

    Burtle _prng;

    TEA _crypto;

    EventQueue<LEGODimensions::EventPacket> _eventQueue;

    /**
     * @brief A struct used for keeping track of placed tokens
     *
     */
    struct PlacedToken {
        uint8_t index;         // the index value sent in HID events
        TagIndex padLocation;  // pad number 1-3
        ToyTag* tag;           // non-owning pointer
    };

    static constexpr uint8_t MAX_TOKENS = 7;

    /**
     * @brief An array/pointer used for storing all of the tokens currently placed on the pad
     *
     */
    PlacedToken _placedTokens[MAX_TOKENS]{};

    /**
     * @brief The amount of currently placed tags inside of _placedTokens
     *
     */
    uint8_t _placedCount = 0x00;

    /**
     * @brief A bitmask for the currently available indexes
     *
     */
    uint8_t _indexMask = 0b00000000;  // bit N set → index N currently in use

    /**
     * @brief Allocates a new index to be used inside of the bitmask  (i.e. during tag placement)
     *
     * @return uint8_t The actual index byte-value
     */
    uint8_t _allocIndex() {
        for (uint8_t i = 0; i < MAX_TOKENS; i++) {
            if (!(_indexMask & (1u << i))) {
                _indexMask |= (1u << i);
                return i;
            }
        }
        return 0xFF;  // table full
    }

    /**
     * @brief Frees an index to be accessible for use (i.e. during tag removal)
     *
     * @param idx An index value to free up for future use
     */
    void _freeIndex(uint8_t idx) { _indexMask &= ~(1u << idx); }

    /**
     * @brief Finds a PlacedToken instance where the tag is equal to a tag placed on the
     *  playpad.
     *
     * @param tag The tag to find
     * @return PlacedToken* A tag found (or a `nullptr`)
     */
    PlacedToken* _findToken(const ToyTag* tag) {
        for (size_t i = 0; i < _placedCount; i++) {
            if (_placedTokens[i].tag == tag) {
                return &_placedTokens[i];
            }
        }

        return nullptr;
    }

    static PadLocation padSectionFromLocation(TagIndex loc) {
        switch (loc) {
            case TagIndex::TopLeft:
            case TagIndex::BottomLeft:
            case TagIndex::CenterLeft:
                return PadLocation::LEFT;
            case TagIndex::Middle:
                return PadLocation::CENTER;
            case TagIndex::TopRight:
            case TagIndex::BottomRight:
            case TagIndex::CenterRight:
                return PadLocation::RIGHT;
            default:
                return PadLocation::ALL;
        }
    }
};