#include "kamenrider/toypad.h"
#include <config/config.h>
#include "log/logger.h"

void KamenRiderToypad::begin() {
    if (config.getUChar("platform", static_cast<uint8_t>(ToypadPlatform::PS3))
        != static_cast<uint8_t>(ToypadPlatform::PS3)) {
        log_warn("[KamenRiderToypad::begin] Persisted platform isn't PS3, forcing PS3 anyway");
    }

    _platform = ToypadPlatform::PS3;

    log_dbg("[KamenRiderToypad::begin] Platform = PS3");

    PortalTransport::begin();
}

void KamenRiderToypad::_configureDevice() {
    TinyUSBDevice.clearConfiguration();
    TinyUSBDevice.setVersion(0x0200);

    TinyUSBDevice.setID(KamenRider::PS3Identifier.VID, KamenRider::PS3Identifier.PID);
    TinyUSBDevice.setDeviceVersion(0x0100);

    // TODO: Eventually, I'd like to check this, but I cba to purchase an actual gate for myself yet
    TinyUSBDevice.setManufacturerDescriptor("Performance Designed Products");
    TinyUSBDevice.setProductDescriptor("Kamen Rider Ride Gate");
    TinyUSBDevice.setSerialDescriptor("0000000000");
}

void KamenRiderToypad::_onReport(uint8_t const* buffer, uint16_t bufsize) {
    switch (static_cast<KamenRider::PacketType>(buffer[0])) {
        case KamenRider::PacketType::COMMAND: {
            KamenRider::CommandPacket cmd_pkt = KamenRider::CommandPacket();
            memcpy(cmd_pkt.data, buffer, KamenRider::PLAYPAD_MAX_PACKET_SIZE);

            if (cmd_pkt.isValid() != Common::PacketValidationError::OK) {
                log_warn("[KamenRiderToypad] Received invalid command packet from host (%d): %s",
                         static_cast<uint8_t>(cmd_pkt.isValid()), cmd_pkt.toHexString());
                return;
            }

            _handleCommandPacket(cmd_pkt);
            break;
        }
        default: {
            char hexString[bufsize * 2 + 1];
            for (size_t i = 0; i < bufsize; ++i) {
                sprintf(&hexString[i * 2], "%02X", buffer[i]);
            }

            log_warn("[KamenRiderToypad] Received unhandled packet type 0x%02X: %s", buffer[0],
                     hexString);
            break;
        }
    }
}

void KamenRiderToypad::_handleCommandPacket(const KamenRider::CommandPacket& packet) {
    log_dbg("[KamenRiderToypad] Valid packet received: Command=0x%02X, PayloadSize=%d, CID=0x%02X",
            packet.command(), packet.payloadSize(), packet.cid());

    auto it = _commandHandlers.find(packet.command());
    if (it != _commandHandlers.end()) {
        it->second(packet);
    } else {
        log_warn("[KamenRiderToypad] Unhandled command received: 0x%02X (Payload: %s)",
                 packet.command(), packet.payloadToHexString());
    }
}

void KamenRiderToypad::_registerHandlers() {
    _commandHandlers[static_cast<uint8_t>(KamenRider::GatewayCommand::WAKE)] =
        [this](const KamenRider::CommandPacket& p) { _handleWake(p); };
    _commandHandlers[static_cast<uint8_t>(KamenRider::GatewayCommand::COL)] =
        [this](const KamenRider::CommandPacket& p) { _handleCol(p); };
    _commandHandlers[static_cast<uint8_t>(KamenRider::GatewayCommand::FLASH)] =
        [this](const KamenRider::CommandPacket& p) { _handleFlash(p); };
    _commandHandlers[static_cast<uint8_t>(KamenRider::GatewayCommand::TGLST)] =
        [this](const KamenRider::CommandPacket& p) { _handleTagList(p); };
    _commandHandlers[static_cast<uint8_t>(KamenRider::GatewayCommand::READ)] =
        [this](const KamenRider::CommandPacket& p) { _handleRead(p); };
    _commandHandlers[static_cast<uint8_t>(KamenRider::GatewayCommand::WRITE)] =
        [this](const KamenRider::CommandPacket& p) { _handleWrite(p); };
}

void KamenRiderToypad::_updateExtra() {
    _eventQueue.update([this](KamenRider::BasePacket& p) { sendPacket(p); });
}

bool KamenRiderToypad::placeToy(Toy* toy, int lastRawLocation) {
    if (toy->toyGame() != ToyGame::KamenRider) {
        log_warn("[KamenRiderToypad::placeToy] Ignoring non-Kamen Rider toy id=%u", toy->toyId());
        return false;
    }

    return tagChangeEvent(static_cast<KamenRiderToyTag*>(toy),
                          static_cast<PadLocation>(lastRawLocation));
}

bool KamenRiderToypad::tagChangeEvent(KamenRiderToyTag* tag, PadLocation lastLocation) {
    const PadLocation newLocation = tag->location();

    if (newLocation == PadLocation::ALL) {
        PlacedTag* placed = _findTag(tag);
        if (placed == nullptr) {
            log_err("[KamenRiderToypad::tagChangeEvent] Remove: tag id=%u not in tracking table",
                    tag->toyId());
            return false;
        }

        log_dbg("[KamenRiderToypad::tagChangeEvent] Tag removed - id=%u pad=%d", tag->toyId(),
                static_cast<int>(lastLocation));

        _queueTagEvent(tag, KamenRider::TagEventPacket::REMOVED);

        *placed = _placedTags[--_placedCount];  // swap-evict
        return true;
    }

    PlacedTag* existing = _findTag(tag);
    if (existing != nullptr) {
        if (existing->location == newLocation) {
            return true;
        }

        log_dbg("[KamenRiderToypad::tagChangeEvent] Tag moved - id=%u pad=%d -> %d", tag->toyId(),
                static_cast<int>(existing->location), static_cast<int>(newLocation));

        // The gate doesn't have any specific location, far as the game is concerned.
        existing->location = newLocation;
        return true;
    }

    if (_placedCount >= MAX_TAGS) {
        log_warn("[KamenRiderToypad::tagChangeEvent] Tag table full, cannot place id=%u",
                 tag->toyId());
        return false;
    }

    _placedTags[_placedCount++] = {newLocation, tag};

    log_dbg("[KamenRiderToypad::tagChangeEvent] Tag placed - id=%u pad=%d", tag->toyId(),
            static_cast<int>(newLocation));

    _queueTagEvent(tag, KamenRider::TagEventPacket::PLACED);

    return true;
}

void KamenRiderToypad::_queueTagEvent(const KamenRiderToyTag* tag,
                                      KamenRider::TagEventPacket::TagMovementDirection direction) {
    if (!_awake) {
        return;
    }

    uint8_t uid[KamenRider::TAG_UID_SIZE];
    tag->getUID(uid);

    KamenRider::EventPacket packet = KamenRider::TagEventPacket::build(uid, direction);
    _eventQueue.push(packet);
}

void KamenRiderToypad::_handleWake(const KamenRider::CommandPacket& packet) {
    log_dbg("[KamenRiderToypad] WAKE command received. Payload: %s", packet.payloadToHexString());

    _awake = true;

    KamenRider::ResponsePacket response = KamenRider::WakePacket::buildResponse(packet.cid());
    sendPacket(response);
}

void KamenRiderToypad::_handleCol(const KamenRider::CommandPacket& packet) {
    const PadColor color = KamenRider::ColorPacket::fromCommand(packet);
    log_dbg("[KamenRiderToypad] COL parsed; (R:%d G:%d B:%d)", color.r, color.g, color.b);

    // The Ridegate has a single light, so it drives every LED section.
    _setColor(PadLocation::ALL, color);

    KamenRider::ResponsePacket response = KamenRider::ColorPacket::buildResponse(packet.cid());
    sendPacket(response);
}

void KamenRiderToypad::_handleFlash(const KamenRider::CommandPacket& packet) {
    const KamenRider::FlashPacket::FlashStatus parsed =
        KamenRider::FlashPacket::fromCommand(packet);
    log_dbg("[KamenRiderToypad] FLASH parsed; OnTicks:%d OffTicks:%d Count:%d (R:%d G:%d B:%d)",
            parsed.onTicks, parsed.offTicks, parsed.count, parsed.offColor.r, parsed.offColor.g,
            parsed.offColor.b);

    for (int i = 0; i < NUM_PADS; i++) {
        PADS[i].setFlash(parsed.offColor, parsed.onTicks, parsed.offTicks, parsed.count);
    }
    _padStateDirty = true;

    KamenRider::ResponsePacket response = KamenRider::FlashPacket::buildResponse(packet.cid());
    sendPacket(response);
}

void KamenRiderToypad::_handleTagList(const KamenRider::CommandPacket& packet) {
    uint8_t uids[MAX_TAGS][KamenRider::TAG_UID_SIZE]{};
    for (uint8_t i = 0; i < _placedCount; i++) {
        _placedTags[i].tag->getUID(uids[i]);
    }

    // TagListPacket only fits TagListPacket::MAX_ENTRIES UIDs, so anything past that is dropped.
    log_dbg("[KamenRiderToypad] TGLST Response - %d tag(s)", _placedCount);
    KamenRider::ResponsePacket response =
        KamenRider::TagListPacket::buildResponse(packet.cid(), uids, _placedCount);
    sendPacket(response);
}

void KamenRiderToypad::_handleRead(const KamenRider::CommandPacket& packet) {
    const KamenRider::TagBlockAddress address = KamenRider::ReadPacket::fromCommand(packet);
    log_dbg("[KamenRiderToypad] READ command - sector=%d block=%d", address.sector, address.block);

    uint8_t data[KamenRider::TAG_BLOCK_SIZE]{};

    PlacedTag* placed = _findTagByUID(address.uid);
    if (placed == nullptr) {
        log_warn("[KamenRiderToypad] READ - no placed tag with the requested UID");
    } else if (!address.isValid()) {
        log_warn("[KamenRiderToypad] READ - sector=%d block=%d out of range", address.sector,
                 address.block);
    } else {
        placed->tag->get(address.page(), data);
    }

    KamenRider::ResponsePacket response = KamenRider::ReadPacket::buildResponse(packet.cid(), data);
    sendPacket(response);
}

void KamenRiderToypad::_handleWrite(const KamenRider::CommandPacket& packet) {
    const KamenRider::WritePacket::WriteRequest req = KamenRider::WritePacket::fromCommand(packet);
    log_dbg("[KamenRiderToypad] WRITE command - sector=%d block=%d", req.address.sector,
            req.address.block);

    KamenRider::ResponsePacket response = KamenRider::WritePacket::buildResponse(packet.cid());
    sendPacket(response);

    PlacedTag* placed = _findTagByUID(req.address.uid);
    if (placed == nullptr) {
        log_warn("[KamenRiderToypad] WRITE - no placed tag with the requested UID");
        return;
    }

    if (!req.address.isValid()) {
        log_warn("[KamenRiderToypad] WRITE - sector=%d block=%d out of range", req.address.sector,
                 req.address.block);
        return;
    }

    placed->tag->set(req.address.page(), req.data);
    log_dbg("[KamenRiderToypad] WRITE - wrote page=%d to tag id=%u", req.address.page(),
            placed->tag->toyId());
    _notifyTagStateChange(placed->tag);
}
