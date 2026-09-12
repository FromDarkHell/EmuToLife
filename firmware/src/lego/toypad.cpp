#include "lego/toypad.h"
#include <config/config.h>
#include "log/logger.h"

Toypad::Toypad() {
    _game = ToyGame::LegoDimensions;
    this->_registerHandlers();
}

void Toypad::begin() {
    // Read persisted platform/game choice; default to PS3 + LEGO Dimensions if never set.
    uint8_t platformVal = config.getUChar("platform", static_cast<uint8_t>(ToypadPlatform::PS3));
    _platform = static_cast<ToypadPlatform>(platformVal);

    log_dbg("[Toypad::begin] Platform = %s", _platform == ToypadPlatform::X360 ? "X360" : "PS3");

    PortalTransport::begin();
}

void Toypad::_onBegin() {
    this->_prng.init(0x00);
    this->_crypto.setKey(LEGODimensions::TEA_KEY);
}

void Toypad::_updateExtra() {
    // Drain one queued event packet per interval tick
    _eventQueue.update([this](LEGODimensions::BasePacket& p) { sendPacket(p); });
}

void Toypad::_configureDevice() {
    TinyUSBDevice.clearConfiguration();
    TinyUSBDevice.setVersion(0x0200);

    switch (_platform) {
        case ToypadPlatform::X360: {
            TinyUSBDevice.setID(LEGODimensions::X360Identifier.VID,
                                LEGODimensions::X360Identifier.PID);
            TinyUSBDevice.setDeviceVersion(LEGODimensions::X360_DEVICE_VERSION);
            TinyUSBDevice.setManufacturerDescriptor(LEGODimensions::X360_MANUFACTURER);
            TinyUSBDevice.setProductDescriptor(LEGODimensions::X360_PRODUCT_NAME);
            TinyUSBDevice.setSerialDescriptor(LEGODimensions::X360_SERIAL);

            break;
        }

        case ToypadPlatform::PS3:
        default: {
            TinyUSBDevice.setID(LEGODimensions::PS3Identifier.VID,
                                LEGODimensions::PS3Identifier.PID);
            TinyUSBDevice.setDeviceVersion(LEGODimensions::PS3_DEVICE_VERSION);
            TinyUSBDevice.setManufacturerDescriptor(LEGODimensions::PS3_MANUFACTURER);
            TinyUSBDevice.setProductDescriptor(LEGODimensions::PS3_PRODUCT_NAME);
            TinyUSBDevice.setSerialDescriptor(LEGODimensions::PS3_SERIAL);
            break;
        }
    }
}

void Toypad::_onReport(uint8_t const* buffer, uint16_t bufsize) {
    switch (static_cast<LEGODimensions::PacketType>(buffer[0])) {
        case LEGODimensions::PacketType::COMMAND: {
            LEGODimensions::CommandPacket cmd_pkt = LEGODimensions::CommandPacket();
            memcpy(cmd_pkt.data, buffer, LEGODimensions::PLAYPAD_MAX_PACKET_SIZE);

            if (cmd_pkt.isValid() != LEGODimensions::PacketValidationError::OK) {
                log_warn("[PlayPad] Received invalid command packet from host (%d): %s",
                         static_cast<uint8_t>(cmd_pkt.isValid()), cmd_pkt.toHexString());
                return;
            }

            _handleCommandPacket(cmd_pkt);
            break;
        }
        case LEGODimensions::PacketType::EVENT: {
            log_warn("[PlayPad] Received Event packet from host, which is unexpected. Ignoring.");
            break;
        }
        default: {
            char hexString[bufsize * 2 + 1];
            for (size_t i = 0; i < bufsize; ++i) {
                sprintf(&hexString[i * 2], "%02X", buffer[i]);
            }

            log_warn("[PlayPad] Received unknown data: %s", hexString);
            break;
        }
    }
}

void Toypad::_handleCommandPacket(const LEGODimensions::CommandPacket& packet) {
    log_dbg("[PlayPad] Valid packet received: Command=0x%02X, PayloadSize=%d, CID=0x%02X",
            packet.command(), packet.payloadSize(), packet.cid());

    auto it = _commandHandlers.find(packet.command());
    if (it != _commandHandlers.end()) {
        it->second(packet);
    } else {
        log_warn("[PlayPad] Unhandled command received: 0x%02X (Payload: %b[32])", packet.command(),
                 packet.payload());
    }
}

void Toypad::_registerHandlers() {
    // Add a new line here to register any new command - nothing else needs to change.
    _commandHandlers[static_cast<uint8_t>(LEGODimensions::GatewayCommand::WAKE)] =
        [this](const LEGODimensions::CommandPacket& p) { _handleWake(p); };
    _commandHandlers[static_cast<uint8_t>(LEGODimensions::GatewayCommand::SEED)] =
        [this](const LEGODimensions::CommandPacket& p) { _handleSeed(p); };
    _commandHandlers[static_cast<uint8_t>(LEGODimensions::GatewayCommand::CHALLENGE)] =
        [this](const LEGODimensions::CommandPacket& p) { _handleChallenge(p); };
    _commandHandlers[static_cast<uint8_t>(LEGODimensions::GatewayCommand::COL)] =
        [this](const LEGODimensions::CommandPacket& p) { _handleCol(p); };
    _commandHandlers[static_cast<uint8_t>(LEGODimensions::GatewayCommand::COLAL)] =
        [this](const LEGODimensions::CommandPacket& p) { _handleColAll(p); };
    _commandHandlers[static_cast<uint8_t>(LEGODimensions::GatewayCommand::FLASH)] =
        [this](const LEGODimensions::CommandPacket& p) { _handleFlash(p); };
    _commandHandlers[static_cast<uint8_t>(LEGODimensions::GatewayCommand::FLSAL)] =
        [this](const LEGODimensions::CommandPacket& p) { _handleFlashAll(p); };
    _commandHandlers[static_cast<uint8_t>(LEGODimensions::GatewayCommand::FADE)] =
        [this](const LEGODimensions::CommandPacket& p) { _handleFade(p); };
    _commandHandlers[static_cast<uint8_t>(LEGODimensions::GatewayCommand::FADAL)] =
        [this](const LEGODimensions::CommandPacket& p) { _handleFadeAll(p); };

    _commandHandlers[static_cast<uint8_t>(LEGODimensions::GatewayCommand::GETCOL)] =
        [this](const LEGODimensions::CommandPacket& p) { _handleGetColor(p); };
    _commandHandlers[static_cast<uint8_t>(LEGODimensions::GatewayCommand::TGLST)] =
        [this](const LEGODimensions::CommandPacket& p) { _handleTagList(p); };

    _commandHandlers[static_cast<uint8_t>(LEGODimensions::GatewayCommand::READ)] =
        [this](const LEGODimensions::CommandPacket& p) { _handleRead(p); };
    _commandHandlers[static_cast<uint8_t>(LEGODimensions::GatewayCommand::MODEL)] =
        [this](const LEGODimensions::CommandPacket& p) { _handleModel(p); };
    _commandHandlers[static_cast<uint8_t>(LEGODimensions::GatewayCommand::WRITE)] =
        [this](const LEGODimensions::CommandPacket& p) { _handleWrite(p); };
}

void Toypad::tagChangeEvent(ToyTag* placedTag, TagIndex lastLocation) {
    uint8_t uid[8];
    placedTag->getUID(uid);

    LEGODimensions::TagEventPacket::TagMovementDirection direction =
        LEGODimensions::TagEventPacket::TagMovementDirection::PLACED;
    TagIndex padIndex = placedTag->padIndex;
    if (placedTag->padIndex == TagIndex::Unplaced && lastLocation != TagIndex::Unplaced) {
        direction = LEGODimensions::TagEventPacket::TagMovementDirection::REMOVED;
        padIndex = lastLocation;
    }

    const PadLocation currentPadSection = padSectionFromLocation(padIndex);
    if (currentPadSection == PadLocation::ALL) {
        log_err("[Toypad::tagChangeEvent] No pad section for padIndex=%d",
                static_cast<int>(padIndex));
        return;
    }

    if (direction == LEGODimensions::TagEventPacket::TagMovementDirection::REMOVED) {
        PlacedToken* pt = _findToken(placedTag);
        if (pt == nullptr) {
            log_err("[Toypad::tagChangeEvent] Remove: tag id=%d not in tracking table",
                    placedTag->id);
            return;
        }

        log_dbg("[Toypad::tagChangeEvent] Tag removed - id=%d index=%d pad=%d", placedTag->id,
                pt->index, static_cast<int>(padIndex));

        LEGODimensions::EventPacket packet =
            LEGODimensions::TagEventPacket::build(currentPadSection, pt->index, direction, uid);
        _eventQueue.push(packet);

        _freeIndex(pt->index);
        *pt = _placedTokens[--_placedCount];  // swap-evict
    } else {
        PlacedToken* existing = _findToken(placedTag);

        if (existing != nullptr) {
            const PadLocation oldSection = padSectionFromLocation(existing->padLocation);

            if (oldSection == currentPadSection) {
                // Same section - positional move, keep index
                log_dbg("[Toypad::tagChangeEvent] Tag moved within section - id=%d index=%d pad=%d",
                        placedTag->id, existing->index, static_cast<int>(currentPadSection));

                existing->padLocation = padIndex;

                LEGODimensions::EventPacket packet = LEGODimensions::TagEventPacket::build(
                    currentPadSection, existing->index, direction, uid);
                _eventQueue.push(packet);
            } else {
                // Cross-section move:
                // Free old index first so the alloc below gets the lowest available,
                // which will be the same index if nothing else is free at a lower slot.
                log_dbg(
                    "[Toypad::tagChangeEvent] Tag crossed sections - id=%d index=%d pad=%d -> %d",
                    placedTag->id, existing->index, static_cast<int>(oldSection),
                    static_cast<int>(currentPadSection));

                const uint8_t oldIndex = existing->index;

                // 1. REMOVE from old section, free the index
                LEGODimensions::EventPacket removePacket = LEGODimensions::TagEventPacket::build(
                    oldSection, oldIndex,
                    LEGODimensions::TagEventPacket::TagMovementDirection::REMOVED, uid);
                _eventQueue.push(removePacket);
                _freeIndex(oldIndex);

                // 2. Alloc new index (lowest free - hopefully the one *just* freed)
                const uint8_t newIndex = _allocIndex();
                if (newIndex == 0xFF) {
                    log_warn(
                        "[Toypad::tagChangeEvent] Index table full during cross-section move, "
                        "id=%d",
                        placedTag->id);
                    // Evict the stale slot so the table stays consistent
                    *existing = _placedTokens[--_placedCount];
                    return;
                }

                // 3. Update slot in-place - no evict/reinsert needed
                existing->index = newIndex;
                existing->padLocation = padIndex;

                // 4. PLACE on new section with new index
                LEGODimensions::EventPacket placePacket = LEGODimensions::TagEventPacket::build(
                    currentPadSection, newIndex,
                    LEGODimensions::TagEventPacket::TagMovementDirection::PLACED, uid);
                _eventQueue.push(placePacket);
            }
        } else {
            // Genuinely new placement
            const uint8_t idx = _allocIndex();
            if (idx == 0xFF) {
                log_warn("[Toypad::tagChangeEvent] Token table full, cannot place id=%d",
                         placedTag->id);
                return;
            }

            _placedTokens[_placedCount++] = {idx, padIndex, placedTag};

            log_dbg("[Toypad::tagChangeEvent] Tag placed - id=%d index=%d pad=%d", placedTag->id,
                    idx, static_cast<int>(currentPadSection));

            LEGODimensions::EventPacket packet =
                LEGODimensions::TagEventPacket::build(currentPadSection, idx, direction, uid);
            _eventQueue.push(packet);
        }
    }
}

bool Toypad::placeToy(Toy* toy, int lastRawLocation) {
    if (toy->toyGame() != ToyGame::LegoDimensions) {
        log_warn("[Toypad::placeToy] Ignoring non-LEGO toy id=%u", toy->toyId());
        return false;
    }

    // tagChangeEvent() has no capacity limit narrower than MAX_TOKENS (checked internally), so
    // there's no rejection case here to report back yet.
    tagChangeEvent(static_cast<ToyTag*>(toy), static_cast<TagIndex>(lastRawLocation));
    return true;
}

void Toypad::_handleWake(const LEGODimensions::CommandPacket& packet) {
    log_dbg("[PlayPad] WAKE command received. Payload: %s", packet.payloadToHexString());

    static const uint8_t WAKE_RESPONSE[] = {0x28, 0x63, 0x29, 0x20, 0x4c, 0x45, 0x47,
                                            0x4f, 0x20, 0x32, 0x30, 0x31, 0x34};

    LEGODimensions::ResponsePacket response =
        LEGODimensions::ResponsePacket::build(packet.cid(), WAKE_RESPONSE, sizeof(WAKE_RESPONSE));
    sendPacket(response);

    // Send tag-placed events for every tag on the playpad currently.
    for (int i = 0; i < _placedCount; i++) {
        this->tagChangeEvent(_placedTokens[i].tag, TagIndex::Unplaced);
    }
}

void Toypad::_handleSeed(const LEGODimensions::CommandPacket& packet) {
    log_dbg("[PlayPad] SEED command received. Payload: %s", packet.payloadToHexString());
    LEGODimensions::SeedPacket::SeedStatus decrypted =
        LEGODimensions::SeedPacket::fromCommand(packet, &_crypto);
    log_dbg("[PlayPad] SEED parsed. (Seed: %d, Config: %d)", decrypted.seed, decrypted.conf);

    _prng.init(decrypted.seed);
    LEGODimensions::ResponsePacket response =
        LEGODimensions::SeedPacket::fromStatus(packet.cid(), decrypted, &_crypto);
    sendPacket(response);
}

void Toypad::_handleChallenge(const LEGODimensions::CommandPacket& packet) {
    log_dbg("[PlayPad] CHALLENGE command received. Payload: %s", packet.payloadToHexString());
    LEGODimensions::ChallengePacket::ChallengeStatus decrypted =
        LEGODimensions::ChallengePacket::fromCommand(packet, &_crypto);
    log_dbg("[PlayPad] CHALLENGE parsed. (Config: %d)", decrypted.conf);

    LEGODimensions::ResponsePacket response = LEGODimensions::ChallengePacket::fromStatus(
        packet.cid(), _prng.rand(), decrypted, &_crypto);
    sendPacket(response);
}

void Toypad::_handleCol(const LEGODimensions::CommandPacket& packet) {
    LEGODimensions::ColorPacket::ColorStatus parsed =
        LEGODimensions::ColorPacket::fromCommand(packet);
    log_dbg("[PlayPad] COL parsed; PadIndex: %d (R:%d G:%d B:%d)",
            static_cast<uint8_t>(parsed.padLocation), parsed.padColor.r, parsed.padColor.g,
            parsed.padColor.b);

    _setColor(parsed.padLocation, parsed.padColor);

    LEGODimensions::ResponsePacket blank = LEGODimensions::ResponsePacket::blank(packet.cid());
    sendPacket(blank);
}

void Toypad::_handleColAll(const LEGODimensions::CommandPacket& packet) {
    LEGODimensions::ColorAllPacket::ColorStatus parsed =
        LEGODimensions::ColorAllPacket::fromCommand(packet);
    log_dbg(
        "[PlayPad] COLAL parsed; Left:(R:%d G:%d B:%d) Mid:(R:%d G:%d B:%d) Right:(R:%d G:%d B:%d)",
        parsed.leftColor.r, parsed.leftColor.g, parsed.leftColor.b, parsed.centerColor.r,
        parsed.centerColor.g, parsed.centerColor.b, parsed.rightColor.r, parsed.rightColor.g,
        parsed.rightColor.b);

    getPad(PadLocation::LEFT)->setColor(parsed.leftColor);
    getPad(PadLocation::CENTER)->setColor(parsed.centerColor);
    getPad(PadLocation::RIGHT)->setColor(parsed.rightColor);

    LEGODimensions::ResponsePacket blank = LEGODimensions::ResponsePacket::blank(packet.cid());
    sendPacket(blank);

    _padStateDirty = true;
}

void Toypad::_handleFlash(const LEGODimensions::CommandPacket& packet) {
    LEGODimensions::FlashPacket::FlashStatus parsed =
        LEGODimensions::FlashPacket::fromCommand(packet);
    log_dbg("[PlayPad] FLASH parsed; PadIndex:%d OnTicks:%d OffTicks:%d (R:%d G:%d B:%d)",
            static_cast<uint8_t>(parsed.padLocation), parsed.onTicks, parsed.offTicks,
            parsed.offColor.r, parsed.offColor.g, parsed.offColor.b);

    if (parsed.padLocation == PadLocation::ALL) {
        for (int i = 0; i < NUM_PADS; i++) {
            PADS[i].setFlash(parsed.offColor, parsed.onTicks, parsed.offTicks, parsed.count);
        }
    } else {
        getPad(parsed.padLocation)
            ->setFlash(parsed.offColor, parsed.onTicks, parsed.offTicks, parsed.count);
    }

    LEGODimensions::ResponsePacket blank = LEGODimensions::ResponsePacket::blank(packet.cid());
    sendPacket(blank);

    _padStateDirty = true;
}

void Toypad::_handleFlashAll(const LEGODimensions::CommandPacket& packet) {
    log_dbg("[PlayPad] FLSAL command received. Payload: %s", packet.payloadToHexString());
    LEGODimensions::FlashAllPacket::FlashStatus parsed =
        LEGODimensions::FlashAllPacket::fromCommand(packet);

    struct {
        PadLocation loc;
        LEGODimensions::FlashAllPacket::FlashStatus::PadFlashStatus& pad;
    } pads[] = {
        {PadLocation::LEFT, parsed.leftPad},
        {PadLocation::CENTER, parsed.centerPad},
        {PadLocation::RIGHT, parsed.rightPad},
    };

    for (auto& [loc, pad] : pads) {
        if (pad.enable) {
            getPad(loc)->setFlash(pad.offColor, pad.onTicks, pad.offTicks, pad.count);
        }
    }

    LEGODimensions::ResponsePacket blank = LEGODimensions::ResponsePacket::blank(packet.cid());
    sendPacket(blank);

    _padStateDirty = true;
}

void Toypad::_handleFade(const LEGODimensions::CommandPacket& packet) {
    log_dbg("[PlayPad] FADE command received. Payload: %s", packet.payloadToHexString());
    LEGODimensions::FadePacket::FadeStatus parsed = LEGODimensions::FadePacket::fromCommand(packet);
    log_dbg("[PlayPad] FADE parsed; PadIndex:%d Cycles:%d Speed:%d (R:%d G:%d B:%d)",
            static_cast<uint8_t>(parsed.padLocation), parsed.cycles, parsed.speed, parsed.color.r,
            parsed.color.g, parsed.color.b);

    if (parsed.padLocation == PadLocation::ALL) {
        for (int i = 0; i < NUM_PADS; i++) {
            PADS[i].setFade(parsed.color, parsed.speed, parsed.cycles);
        }
    } else {
        getPad(parsed.padLocation)->setFade(parsed.color, parsed.speed, parsed.cycles);
    }

    LEGODimensions::ResponsePacket blank = LEGODimensions::ResponsePacket::blank(packet.cid());
    sendPacket(blank);

    _padStateDirty = true;
}

void Toypad::_handleFadeAll(const LEGODimensions::CommandPacket& packet) {
    log_dbg("[PlayPad] FADEALL command received. Payload: %s", packet.payloadToHexString());
    LEGODimensions::FadeAllPacket::FadeStatus parsed =
        LEGODimensions::FadeAllPacket::fromCommand(packet);

    struct {
        PadLocation loc;
        LEGODimensions::FadeAllPacket::FadeStatus::PadFadeStatus pad;
    } pads[] = {
        {PadLocation::LEFT, parsed.leftPad},
        {PadLocation::CENTER, parsed.centerPad},
        {PadLocation::RIGHT, parsed.rightPad},
    };

    for (auto& [loc, pad] : pads) {
        if (pad.enable) {
            getPad(loc)->setFade(pad.color, pad.speed, pad.cycles);
        }
    }

    LEGODimensions::ResponsePacket blank = LEGODimensions::ResponsePacket::blank(packet.cid());
    sendPacket(blank);

    _padStateDirty = true;
}

void Toypad::_handleGetColor(const LEGODimensions::CommandPacket& packet) {
    const PadLocation colorToGet = LEGODimensions::GetColorPacket::fromCommand(packet);

    if (colorToGet == PadLocation::ALL) {
        log_warn("[PlayPad] GETCOL received for ALL, ignoring");
        return;
    }

    log_dbg("[PlayPad] Getting color for pad %d", colorToGet);
    LEGODimensions::ResponsePacket response =
        LEGODimensions::GetColorPacket::buildResponse(packet.cid(), getPad(colorToGet)->color());

    sendPacket(response);
}

void Toypad::_handleTagList(const LEGODimensions::CommandPacket& packet) {
    LEGODimensions::TagListPacket::TagEntry tagEntries[MAX_TOKENS]{};

    for (uint8_t i = 0; i < _placedCount; i++) {
        PlacedToken* tag = &_placedTokens[i];
        tagEntries[i] = {tag->index, padSectionFromLocation(tag->padLocation)};
    }

    LEGODimensions::ResponsePacket response =
        LEGODimensions::TagListPacket::buildResponse(packet.cid(), tagEntries, _placedCount);
    log_dbg("[PlayPad] TGLST Response");
    sendPacket(response);
}

void Toypad::_handleRead(const LEGODimensions::CommandPacket& packet) {
    const LEGODimensions::ReadPacket::ReadRequest req =
        LEGODimensions::ReadPacket::fromCommand(packet);

    log_dbg("[PlayPad] READ command // index=%d page=%d", req.index, req.page);

    // Find the placed token matching this index
    const PlacedToken* pt = nullptr;
    for (uint8_t i = 0; i < _placedCount; i++) {
        if (_placedTokens[i].index == req.index) {
            pt = &_placedTokens[i];
            break;
        }
    }

    if (pt == nullptr) {
        log_warn("[PlayPad] READ - no token available at index=%d", req.index);
        return;
    }

    LEGODimensions::ResponsePacket response =
        LEGODimensions::ReadPacket::buildResponse(packet.cid(), pt->tag->_buf, req.page);
    log_dbg("[PlayPad] READ - // Responding with tag id=%d page=%d", pt->tag->id, req.page);
    sendPacket(response);
}

void Toypad::_handleModel(const LEGODimensions::CommandPacket& packet) {
    const LEGODimensions::ModelPacket::ModelRequest req =
        LEGODimensions::ModelPacket::fromCommand(packet, &_crypto);

    log_dbg("[PlayPad] MODEL command - index=%d conf=0x%08X", req.index, req.conf);

    // Find the placed token matching this index
    const PlacedToken* pt = nullptr;
    for (uint8_t i = 0; i < _placedCount; i++) {
        if (_placedTokens[i].index == req.index) {
            pt = &_placedTokens[i];
            break;
        }
    }

    if (pt == nullptr) {
        log_warn("[PlayPad] MODEL - no token at index=%d", req.index);
        LEGODimensions::ResponsePacket err = LEGODimensions::ModelPacket::buildResponse(
            packet.cid(), 0, req.conf, LEGODimensions::ModelPacket::STATUS_NO_TOKEN, &_crypto);
        sendPacket(err);
        return;
    }

    const uint16_t tagId = pt->tag->id;

    if (tagId == 0) {
        log_warn("[PlayPad] MODEL - token at index=%d has no id", req.index);
        LEGODimensions::ResponsePacket noId = LEGODimensions::ModelPacket::buildResponse(
            packet.cid(), 0, req.conf, LEGODimensions::ModelPacket::STATUS_NO_ID, &_crypto);
        sendPacket(noId);
        return;
    }

    log_dbg("[PlayPad] MODEL - responding with id=%d for index=%d", tagId, req.index);
    LEGODimensions::ResponsePacket response = LEGODimensions::ModelPacket::buildResponse(
        packet.cid(), tagId, req.conf, LEGODimensions::ModelPacket::STATUS_OK, &_crypto);
    sendPacket(response);
}

void Toypad::_handleWrite(const LEGODimensions::CommandPacket& packet) {
    const LEGODimensions::WritePacket::WriteRequest req =
        LEGODimensions::WritePacket::fromCommand(packet);

    log_dbg("[PlayPad] WRITE command - index=%d page=%d data=%02X%02X%02X%02X", req.index, req.page,
            req.data[0], req.data[1], req.data[2], req.data[3]);

    LEGODimensions::ResponsePacket response =
        LEGODimensions::WritePacket::buildResponse(packet.cid());
    sendPacket(response);

    PlacedToken* pt = nullptr;
    for (uint8_t i = 0; i < _placedCount; i++) {
        if (_placedTokens[i].index == req.index) {
            pt = &_placedTokens[i];
            break;
        }
    }

    if (pt == nullptr) {
        log_warn("[PlayPad] WRITE - no token at index=%d", req.index);
        return;
    }

    // Write the primary page
    const uint8_t byteOffset = req.page * Tags::NTAG213::PAGE_SIZE;
    if (byteOffset + Tags::NTAG213::PAGE_SIZE > Tags::NTAG213::TAG_SIZE) {
        log_warn("[PlayPad] WRITE - page=%d out of range for index=%d", req.page, req.index);
        return;
    }

    memcpy(pt->tag->_buf + byteOffset, req.data, Tags::NTAG213::PAGE_SIZE);

    // Mirror to the paired page so reads/writes on either offset stay consistent
    // 23 <-> 35  |  24 <-> 36  |  25 <-> 37
    uint8_t mirrorPage = 0xFF;
    switch (req.page) {
        case 23:
            mirrorPage = 35;
            break;
        case 35:
            mirrorPage = 23;
            break;
        case 24:
            mirrorPage = 36;
            break;
        case 36:
            mirrorPage = 24;
            break;
        case 25:
            mirrorPage = 37;
            break;
        case 37:
            mirrorPage = 25;
            break;
        default:
            break;
    }

    if (mirrorPage != 0xFF) {
        const uint8_t mirrorOffset = mirrorPage * Tags::NTAG213::PAGE_SIZE;
        memcpy(pt->tag->_buf + mirrorOffset, req.data, Tags::NTAG213::PAGE_SIZE);
        log_dbg("[PlayPad] WRITE - mirrored page=%d -> page=%d", req.page, mirrorPage);
    }

    log_dbg("[PlayPad] WRITE - wrote page=%d to tag id=%d", req.page, pt->tag->id);

    if (pt->tag->type == ToyTagType::Vehicle) {
        VehicleTag* vehicle = static_cast<VehicleTag*>(pt->tag);

        switch (req.page) {
            case 23:
            case 35:
            case 24:
            case 36:
            case 25:
            case 37:
                vehicle->readFromBuffer();
                log_dbg(
                    "[PlayPad] WRITE - vehicle fields resynced: id=%d upg23=0x%08X upg25=0x%08X",
                    vehicle->id, vehicle->upgradesP23, vehicle->upgradesP25);
                break;
            default:
                break;
        }
    }

    this->_notifyTagStateChange(pt->tag);
}