#pragma once

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <functional>
#include <crypto/burtle.h>
#include <crypto/tea.h>
#include <lego/packets.h>
#include <lego/packet.h>
#include "lego/hid/pad.h"
#include "lego/hid/reports.h"
#include "lego/hid/x360_usb.h"
#include "lego/toy_tag.h"
#include "lego/util/event_queue.h"
#include "log/logger.h"

class Toypad {
   public:
    Toypad();

    /**
     * @brief Initializes the PlayPad device and configures the USB HID interface. This should be
     * called in the setup() function of the main program. After calling this function, the device
     * should be ready to send and receive HID reports with the host.
     *
     */
    void begin();

    /**
     * @brief Updates various playpad state info (i.e. Flashing/Fading LEDs)
     *
     */
    void update();

    /**
     * @brief Returns the active platform this Toypad is presenting as.
     */
    ToypadPlatform platform() const { return _platform; }

    /**
     * @brief Changes the active platform and re-initialises the USB
     *        interface with the matching VID/PID.
     *
     *  The new value is NOT automatically persisted to Config.
     *
     * @param p New platform.
     */
    void setPlatform(ToypadPlatform p);

    PlaypadPad* getPad(PadLocation loc) {
        if (loc == PadLocation::ALL) {
            return nullptr;
        }

        return &PADS[static_cast<uint8_t>(loc) - 1];
    }

    inline void sendPacket(LEGODimensions::BasePacket& packet) {
        log_dbg("[Playpad] Sending packet data: %s", packet.toHexString());

        uint8_t buffer[LEGODimensions::PLAYPAD_MAX_PACKET_SIZE]{};

        if (_platform == ToypadPlatform::X360) {
            // Prepend [0x0B][0x16] and then shift packet content right by 2 bytes.
            buffer[0] = 0x0B;
            buffer[1] = 0x16;
            memcpy(buffer + 2, packet.data, LEGODimensions::PLAYPAD_MAX_PACKET_SIZE - 2);
        } else {
            memcpy(buffer, packet.data, LEGODimensions::PLAYPAD_MAX_PACKET_SIZE);
        }

        _sendReport(buffer, LEGODimensions::PLAYPAD_MAX_PACKET_SIZE);
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

    /**
     * @brief Registers a callback that fires whenever any pad's display state
     *        changes (color, fade, flash).
     *
     * @param cb  Called with a pre-serialized JSON string of the full pad state.
     *            Invoked from the main loop (update()), never from a USB callback.
     */
    void onPadStateChange(std::function<void(const String&)> cb) { _padStateCallback = cb; }

    /**
     * @brief Registers a callback that fires whenever any tag data is updated
     *
     * @param cb  Called with an instance of the modified ToyTag
     */
    void onTagStateChange(std::function<void(const ToyTag*)> cb) { _tagStateCallback = cb; }

   private:
    /**
     * @brief Current platform that the toypad is configured for
     *
     */
    ToypadPlatform _platform = ToypadPlatform::PS3;

    /**
     * @brief The Adafruit_USBD_HID instance that manages the USB HID interface. This is configured
     * to use the report descriptor defined above, and will handle the USB communication with the
     * host.
     *
     */
    Adafruit_USBD_HID _usb_hid;
    X360PortalUSB _x360_usb;

    /**
     * @brief A simple boolean state for whether or not we're already sending a packet, this
     * shouldn't be too important in practice.
     *
     */
    bool _isSending = false;

    /**
     * @brief Configures the USB HID device with the correct vendor ID, product ID, and other device
     * properties.
     *
     */
    void _configureDevice();

    /**
     * @brief Reenumerates the USB device to make sure the host recognizes any changes in device
     * configuration.
     *
     */
    void _reenumerate();

    /**
     * @brief Sends a HID report to the host. The buffer should already be in the format expected by
     * the PlayPad, so the caller is responsible for constructing the packet correctly.
     *
     * @param buffer The buffer containing the HID report to send. This should already be in the
     * format expected by the PlayPad, including any necessary headers, checksums, etc.
     * @param bufsize Size of the buffer to send. This should not exceed PLAYPAD_MAX_PACKET_SIZE.
     * @return true
     * @return false
     */
    bool _sendReport(uint8_t const* buffer, uint16_t bufsize);

    /**
     * @brief Static callback function that gets called when the host requests a HID report. This
     * function should fill the provided buffer with the requested report data, and return the
     * length of the report. The report_id and report_type parameters can be used to determine which
     * report is being requested, if multiple reports are supported.
     *
     * @param report_id
     * @param report_type
     * @param buffer
     * @param reqlen
     * @return uint16_t
     */
    static uint16_t _getReportCallback(uint8_t report_id,
                                       hid_report_type_t report_type,
                                       uint8_t* buffer,
                                       uint16_t reqlen);

    /**
     * @brief Static callback function that gets called when the host sends a HID report to the
     * device. This function should process the received report data as needed. The report_id and
     * report_type parameters can be used to determine which report is being sent, if multiple
     * reports are supported.
     *
     * @param report_id
     * @param report_type
     * @param buffer
     * @param bufsize
     */
    static void _setReportCallback(uint8_t report_id,
                                   hid_report_type_t report_type,
                                   uint8_t const* buffer,
                                   uint16_t bufsize);

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

    PlaypadPad PADS[3] = {
        PlaypadPad(PadLocation::LEFT),
        PlaypadPad(PadLocation::CENTER),
        PlaypadPad(PadLocation::RIGHT),
    };

    Burtle _prng;

    TEA _crypto;

    EventQueue _eventQueue;

    std::function<void(const String&)> _padStateCallback = nullptr;
    bool _padStateDirty = false;

    /**
     * @brief An internal function used for updating the pad state after any major change
     * (colors/etc)
     *
     */
    void _notifyPadStateChange();

    std::function<void(const ToyTag*)> _tagStateCallback = nullptr;
    bool _tagStateDirty = false;

    /**
     * @brief An internal function used for updating the tag state after any major change (writes,
     * etc)
     *
     * @param updatedTag A non-owning pointer reference to the updated tag
     */
    void _notifyTagStateChange(const ToyTag* updatedTag);

    /**
     * @brief A static pointer to the single instance of the PlayPad class. This is used in the
     * static callback functions to access the instance's member variables and functions, since the
     * callbacks themselves cannot be member functions.
     *
     */
    static Toypad* _instance;

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
                return PadLocation::ALL;  // sentinel for "unknown"
        }
    }
};