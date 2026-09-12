#include "infinity/toypad.h"
#include <config/config.h>
#include "log/logger.h"

void InfinityToypad::begin() {
    uint8_t platformVal = config.getUChar("platform", static_cast<uint8_t>(ToypadPlatform::PS3));
    _platform = static_cast<ToypadPlatform>(platformVal);

    log_dbg("[InfinityToypad::begin] Platform = %s",
            _platform == ToypadPlatform::X360 ? "X360" : "PS3");

    PortalTransport::begin();
}

void InfinityToypad::_onBegin() {
    // Seeded for real once a SEED command comes in
    // but just throw it a default value for now
    _prng.init(0x00);
}

void InfinityToypad::_configureDevice() {
    TinyUSBDevice.clearConfiguration();
    TinyUSBDevice.setVersion(0x0200);

    switch (_platform) {
        case ToypadPlatform::X360:
            TinyUSBDevice.setID(DisneyInfinity::X360Identifier.VID,
                                DisneyInfinity::X360Identifier.PID);
            TinyUSBDevice.setDeviceVersion(0x0100);
            // TODO: This is prob wrong lol
            TinyUSBDevice.setManufacturerDescriptor("Disney Interactive");
            TinyUSBDevice.setProductDescriptor("Disney Infinity Reader");
            TinyUSBDevice.setSerialDescriptor("7A11E6F9");
            break;

        case ToypadPlatform::PS3:
        default:
            TinyUSBDevice.setID(DisneyInfinity::PS3Identifier.VID,
                                DisneyInfinity::PS3Identifier.PID);
            TinyUSBDevice.setDeviceVersion(0x0100);
            TinyUSBDevice.setManufacturerDescriptor("Disney Interactive");
            TinyUSBDevice.setProductDescriptor("Disney Infinity Reader");
            TinyUSBDevice.setSerialDescriptor("989A46633837 Performance Designed Products");

            break;
    }
}

void InfinityToypad::_onReport(uint8_t const* buffer, uint16_t bufsize) {
    switch (static_cast<DisneyInfinity::PacketType>(buffer[0])) {
        case DisneyInfinity::PacketType::COMMAND: {
            DisneyInfinity::CommandPacket cmd_pkt = DisneyInfinity::CommandPacket();
            memcpy(cmd_pkt.data, buffer, DisneyInfinity::PLAYPAD_MAX_PACKET_SIZE);

            if (cmd_pkt.isValid() != Common::PacketValidationError::OK) {
                log_warn("[InfinityToypad] Received invalid command packet from host (%d): %s",
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

            log_warn("[InfinityToypad] Received unhandled packet type 0x%02X: %s", buffer[0],
                     hexString);
            break;
        }
    }
}

void InfinityToypad::_handleCommandPacket(const DisneyInfinity::CommandPacket& packet) {
    log_dbg("[InfinityToypad] Valid packet received: Command=0x%02X, PayloadSize=%d, CID=0x%02X",
            packet.command(), packet.payloadSize(), packet.cid());

    auto it = _commandHandlers.find(packet.command());
    if (it != _commandHandlers.end()) {
        it->second(packet);
    } else {
        log_warn("[InfinityToypad] Unhandled command received: 0x%02X (Payload: %b[32])",
                 packet.command(), packet.payload());
    }
}

void InfinityToypad::_registerHandlers() {
    _commandHandlers[static_cast<uint8_t>(DisneyInfinity::GatewayCommand::WAKE)] =
        [this](const DisneyInfinity::CommandPacket& p) { _handleWake(p); };
    _commandHandlers[static_cast<uint8_t>(DisneyInfinity::GatewayCommand::SEED)] =
        [this](const DisneyInfinity::CommandPacket& p) { _handleSeed(p); };
    _commandHandlers[static_cast<uint8_t>(DisneyInfinity::GatewayCommand::CHALLENGE)] =
        [this](const DisneyInfinity::CommandPacket& p) { _handleChallenge(p); };
    _commandHandlers[static_cast<uint8_t>(DisneyInfinity::GatewayCommand::COL)] =
        [this](const DisneyInfinity::CommandPacket& p) { _handleCol(p); };
    _commandHandlers[static_cast<uint8_t>(DisneyInfinity::GatewayCommand::FADE)] =
        [this](const DisneyInfinity::CommandPacket& p) { _handleFade(p); };
    _commandHandlers[static_cast<uint8_t>(DisneyInfinity::GatewayCommand::FADAL)] =
        [this](const DisneyInfinity::CommandPacket& p) { _handleFadeAll(p); };
    _commandHandlers[static_cast<uint8_t>(DisneyInfinity::GatewayCommand::FLASH)] =
        [this](const DisneyInfinity::CommandPacket& p) { _handleFlash(p); };
    _commandHandlers[static_cast<uint8_t>(DisneyInfinity::GatewayCommand::FLSAL)] =
        [this](const DisneyInfinity::CommandPacket& p) { _handleFlashAll(p); };
    _commandHandlers[static_cast<uint8_t>(DisneyInfinity::GatewayCommand::UNK95)] =
        [this](const DisneyInfinity::CommandPacket& p) { _handleUnk95(p); };
    _commandHandlers[static_cast<uint8_t>(DisneyInfinity::GatewayCommand::STATUS)] =
        [this](const DisneyInfinity::CommandPacket& p) { _handleStatus(p); };
    _commandHandlers[static_cast<uint8_t>(DisneyInfinity::GatewayCommand::GETTOYS)] =
        [this](const DisneyInfinity::CommandPacket& p) { _handleGetToys(p); };
    _commandHandlers[static_cast<uint8_t>(DisneyInfinity::GatewayCommand::READ)] =
        [this](const DisneyInfinity::CommandPacket& p) { _handleRead(p); };
    _commandHandlers[static_cast<uint8_t>(DisneyInfinity::GatewayCommand::WRITE)] =
        [this](const DisneyInfinity::CommandPacket& p) { _handleWrite(p); };
    _commandHandlers[static_cast<uint8_t>(DisneyInfinity::GatewayCommand::MODEL)] =
        [this](const DisneyInfinity::CommandPacket& p) { _handleModel(p); };
}

void InfinityToypad::_updateExtra() {
    // Drain one queued figure-event packet per interval tick
    _eventQueue.update([this](DisneyInfinity::BasePacket& p) { sendPacket(p); });
}

bool InfinityToypad::placeToy(Toy* toy, int lastRawLocation) {
    if (toy->toyGame() != ToyGame::DisneyInfinity) {
        log_warn("[InfinityToypad::placeToy] Ignoring non-Infinity toy id=%u", toy->toyId());
        return false;
    }

    return figureChangeEvent(static_cast<InfinityToyTag*>(toy),
                             static_cast<PadLocation>(lastRawLocation));
}

bool InfinityToypad::figureChangeEvent(InfinityToyTag* tag, PadLocation lastLocation) {
    const PadLocation newLocation = tag->location();

    if (newLocation == PadLocation::ALL) {
        // Removal
        PlacedFigure* pf = _findFigure(tag);
        if (pf == nullptr) {
            log_err(
                "[InfinityToypad::figureChangeEvent] Remove: figure id=%u not in tracking table",
                tag->toyId());
            return false;
        }

        log_dbg("[InfinityToypad::figureChangeEvent] Figure removed - id=%u order=%d pad=%d",
                tag->toyId(), pf->orderAdded, static_cast<int>(lastLocation));

        DisneyInfinity::EventPacket packet = DisneyInfinity::FigureEventPacket::build(
            lastLocation, pf->orderAdded, DisneyInfinity::FigureEventPacket::REMOVED);
        _eventQueue.push(packet);

        _freeOrder(pf->orderAdded);
        *pf = _placedFigures[--_placedCount];  // swap-evict
        return true;
    }

    PlacedFigure* existing = _findFigure(tag);

    if (existing != nullptr) {
        if (existing->location == newLocation) {
            return true;
        }

        if (_countAtLocation(newLocation) >= MAX_FIGURES_PER_LOCATION) {
            log_warn(
                "[InfinityToypad::figureChangeEvent] Pad section %d already has %d figures "
                "stacked, cannot move id=%u there",
                static_cast<int>(newLocation), MAX_FIGURES_PER_LOCATION, tag->toyId());
            return false;
        }

        log_dbg("[InfinityToypad::figureChangeEvent] Figure moved // id=%u order=%d pad=%d -> %d",
                tag->toyId(), existing->orderAdded, static_cast<int>(existing->location),
                static_cast<int>(newLocation));

        const uint8_t oldOrder = existing->orderAdded;

        DisneyInfinity::EventPacket removePacket = DisneyInfinity::FigureEventPacket::build(
            existing->location, oldOrder, DisneyInfinity::FigureEventPacket::REMOVED);
        _eventQueue.push(removePacket);
        _freeOrder(oldOrder);

        const uint8_t newOrder = _allocOrder();
        if (newOrder == 0xFF) {
            log_warn("[InfinityToypad::figureChangeEvent] Order table full during move, id=%u",
                     tag->toyId());
            *existing = _placedFigures[--_placedCount];  // evict the stale slot
            return false;
        }

        existing->orderAdded = newOrder;
        existing->location = newLocation;

        DisneyInfinity::EventPacket placePacket = DisneyInfinity::FigureEventPacket::build(
            newLocation, newOrder, DisneyInfinity::FigureEventPacket::PLACED);
        _eventQueue.push(placePacket);
        return true;
    }

    if (_placedCount >= MAX_FIGURES) {
        log_warn("[InfinityToypad::figureChangeEvent] Figure table full, cannot place id=%u",
                 tag->toyId());
        return false;
    }

    if (_countAtLocation(newLocation) >= MAX_FIGURES_PER_LOCATION) {
        log_warn(
            "[InfinityToypad::figureChangeEvent] Pad section %d already has %d figures stacked, "
            "cannot place id=%u there",
            static_cast<int>(newLocation), MAX_FIGURES_PER_LOCATION, tag->toyId());
        return false;
    }

    const uint8_t order = _allocOrder();
    if (order == 0xFF) {
        log_warn("[InfinityToypad::figureChangeEvent] Order table full, cannot place id=%u",
                 tag->toyId());
        return false;
    }

    _placedFigures[_placedCount++] = {order, newLocation, tag};

    log_dbg("[InfinityToypad::figureChangeEvent] Figure placed - id=%u order=%d pad=%d",
            tag->toyId(), order, static_cast<int>(newLocation));

    DisneyInfinity::EventPacket packet = DisneyInfinity::FigureEventPacket::build(
        newLocation, order, DisneyInfinity::FigureEventPacket::PLACED);
    _eventQueue.push(packet);
    return true;
}

void InfinityToypad::_handleGetToys(const DisneyInfinity::CommandPacket& packet) {
    DisneyInfinity::GetToysPacket::FigureEntry entries[MAX_FIGURES]{};

    for (uint8_t i = 0; i < _placedCount; i++) {
        entries[i] = {_placedFigures[i].orderAdded, _placedFigures[i].location};
    }

    DisneyInfinity::ResponsePacket response =
        DisneyInfinity::GetToysPacket::buildResponse(packet.cid(), entries, _placedCount);
    log_dbg("[InfinityToypad] GETTOYS Response - %d figure(s)", _placedCount);
    sendPacket(response);
}

void InfinityToypad::_handleRead(const DisneyInfinity::CommandPacket& packet) {
    const DisneyInfinity::ReadPacket::ReadRequest req =
        DisneyInfinity::ReadPacket::fromCommand(packet);

    log_dbg("[InfinityToypad] READ command // figureIndex=%d block=%d", req.figureIndex, req.block);

    PlacedFigure* pf = _findByOrder(req.figureIndex);
    if (pf == nullptr) {
        log_warn("[InfinityToypad] READ - no figure at index=%d", req.figureIndex);
        return;
    }

    uint8_t data[DisneyInfinity::ReadPacket::DATA_SIZE]{};
    if (!pf->tag->getBlock(req.block, data)) {
        log_warn("[InfinityToypad] READ - block=%d out of range for index=%d", req.block,
                 req.figureIndex);
        return;
    }

    DisneyInfinity::ResponsePacket response =
        DisneyInfinity::ReadPacket::buildResponse(packet.cid(), data);
    log_dbg("[InfinityToypad] READ - // Responding with figure id=%u block=%d", pf->tag->toyId(),
            req.block);
    sendPacket(response);
}

void InfinityToypad::_handleWrite(const DisneyInfinity::CommandPacket& packet) {
    const DisneyInfinity::WritePacket::WriteRequest req =
        DisneyInfinity::WritePacket::fromCommand(packet);

    log_dbg("[InfinityToypad] WRITE command - figureIndex=%d block=%d", req.figureIndex, req.block);

    // Ack immediately, matching Toypad's WRITE precedent for hardware timing expectations.
    DisneyInfinity::ResponsePacket response =
        DisneyInfinity::WritePacket::buildResponse(packet.cid());
    sendPacket(response);

    PlacedFigure* pf = _findByOrder(req.figureIndex);
    if (pf == nullptr) {
        log_warn("[InfinityToypad] WRITE - no figure at index=%d", req.figureIndex);
        return;
    }

    if (!pf->tag->setBlock(req.block, req.data)) {
        log_warn("[InfinityToypad] WRITE - block=%d out of range for index=%d", req.block,
                 req.figureIndex);
        return;
    }

    log_dbg("[InfinityToypad] WRITE - wrote block=%d to figure id=%u", req.block, pf->tag->toyId());
    this->_notifyTagStateChange(pf->tag);
}

void InfinityToypad::_handleModel(const DisneyInfinity::CommandPacket& packet) {
    const uint8_t figureIndex = DisneyInfinity::ModelPacket::fromCommand(packet);

    log_dbg("[InfinityToypad] MODEL command - figureIndex=%d", figureIndex);

    PlacedFigure* pf = _findByOrder(figureIndex);
    if (pf == nullptr) {
        log_warn("[InfinityToypad] MODEL - no figure at index=%d", figureIndex);
        uint8_t zeroUid[DisneyInfinity::ModelPacket::UID_SIZE]{};
        DisneyInfinity::ResponsePacket response =
            DisneyInfinity::ModelPacket::buildResponse(packet.cid(), zeroUid);
        sendPacket(response);
        return;
    }

    uint8_t uid[DisneyInfinity::ModelPacket::UID_SIZE];
    pf->tag->getUID(uid);

    log_dbg("[InfinityToypad] MODEL - responding with UID for index=%d", figureIndex);
    DisneyInfinity::ResponsePacket response =
        DisneyInfinity::ModelPacket::buildResponse(packet.cid(), uid);
    sendPacket(response);
}

void InfinityToypad::_handleWake(const DisneyInfinity::CommandPacket& packet) {
    log_dbg("[InfinityToypad] WAKE command received. Payload: %s", packet.payloadToHexString());

    DisneyInfinity::ResponsePacket response = DisneyInfinity::WakePacket::buildResponse();
    sendPacket(response);

    // Send figure-placed events for every figure already on the pad, same as Toypad's WAKE.
    for (uint8_t i = 0; i < _placedCount; i++) {
        DisneyInfinity::EventPacket placePacket = DisneyInfinity::FigureEventPacket::build(
            _placedFigures[i].location, _placedFigures[i].orderAdded,
            DisneyInfinity::FigureEventPacket::PLACED);
        _eventQueue.push(placePacket);
    }
}

void InfinityToypad::_handleSeed(const DisneyInfinity::CommandPacket& packet) {
    log_dbg("[InfinityToypad] SEED command received. Payload: %s", packet.payloadToHexString());

    const uint32_t seed = DisneyInfinity::SeedPacket::fromCommand(packet);
    log_dbg("[InfinityToypad] SEED parsed. (Seed: %u)", seed);

    _prng.init(seed);

    DisneyInfinity::ResponsePacket response =
        DisneyInfinity::SeedPacket::buildResponse(packet.cid());
    sendPacket(response);
}

void InfinityToypad::_handleChallenge(const DisneyInfinity::CommandPacket& packet) {
    log_dbg("[InfinityToypad] CHALLENGE command received.");

    DisneyInfinity::ResponsePacket response =
        DisneyInfinity::ChallengePacket::buildResponse(packet.cid(), _prng.next());
    sendPacket(response);
}

void InfinityToypad::_handleCol(const DisneyInfinity::CommandPacket& packet) {
    DisneyInfinity::ColorPacket::ColorStatus parsed =
        DisneyInfinity::ColorPacket::fromCommand(packet);
    log_dbg("[InfinityToypad] COL parsed; PadIndex: %d (R:%d G:%d B:%d)",
            static_cast<uint8_t>(parsed.padLocation), parsed.padColor.r, parsed.padColor.g,
            parsed.padColor.b);

    _setColor(parsed.padLocation, parsed.padColor);

    DisneyInfinity::ResponsePacket response =
        DisneyInfinity::ColorPacket::buildResponse(packet.cid());
    sendPacket(response);
}

void InfinityToypad::_handleFade(const DisneyInfinity::CommandPacket& packet) {
    DisneyInfinity::FadePacket::FadeStatus parsed = DisneyInfinity::FadePacket::fromCommand(packet);
    log_dbg("[InfinityToypad] FADE parsed; PadIndex:%d Speed:%d Cycles:%d (R:%d G:%d B:%d)",
            static_cast<uint8_t>(parsed.padLocation), parsed.speed, parsed.cycles, parsed.color.r,
            parsed.color.g, parsed.color.b);

    if (parsed.padLocation == PadLocation::ALL) {
        for (int i = 0; i < NUM_PADS; i++) {
            PADS[i].setFade(parsed.color, parsed.speed, parsed.cycles);
        }
    } else {
        getPad(parsed.padLocation)->setFade(parsed.color, parsed.speed, parsed.cycles);
    }

    DisneyInfinity::ResponsePacket response =
        DisneyInfinity::FadePacket::buildResponse(packet.cid());
    sendPacket(response);

    _padStateDirty = true;
}

void InfinityToypad::_handleFadeAll(const DisneyInfinity::CommandPacket& packet) {
    DisneyInfinity::FadeAllPacket::FadeStatus parsed =
        DisneyInfinity::FadeAllPacket::fromCommand(packet);

    struct {
        PadLocation loc;
        DisneyInfinity::FadeAllPacket::FadeStatus::PadFadeStatus& pad;
    } pads[] = {
        {PadLocation::CENTER, parsed.centerPad},
        {PadLocation::LEFT, parsed.leftPad},
        {PadLocation::RIGHT, parsed.rightPad},
    };

    for (auto& [loc, pad] : pads) {
        if (pad.enable) {
            getPad(loc)->setFade(pad.color, pad.speed, pad.cycles);
        }
    }

    DisneyInfinity::ResponsePacket response =
        DisneyInfinity::FadeAllPacket::buildResponse(packet.cid());
    sendPacket(response);

    _padStateDirty = true;
}

void InfinityToypad::_handleFlash(const DisneyInfinity::CommandPacket& packet) {
    DisneyInfinity::FlashPacket::FlashStatus parsed =
        DisneyInfinity::FlashPacket::fromCommand(packet);
    log_dbg("[InfinityToypad] FLASH parsed; PadIndex:%d OnTicks:%d OffTicks:%d (R:%d G:%d B:%d)",
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

    DisneyInfinity::ResponsePacket response =
        DisneyInfinity::FlashPacket::buildResponse(packet.cid());
    sendPacket(response);

    _padStateDirty = true;
}

void InfinityToypad::_handleFlashAll(const DisneyInfinity::CommandPacket& packet) {
    DisneyInfinity::FlashAllPacket::FlashStatus parsed =
        DisneyInfinity::FlashAllPacket::fromCommand(packet);

    struct {
        PadLocation loc;
        DisneyInfinity::FlashAllPacket::FlashStatus::PadFlashStatus& pad;
    } pads[] = {
        {PadLocation::CENTER, parsed.centerPad},
        {PadLocation::LEFT, parsed.leftPad},
        {PadLocation::RIGHT, parsed.rightPad},
    };

    for (auto& [loc, pad] : pads) {
        if (pad.enable) {
            getPad(loc)->setFlash(pad.offColor, pad.onTicks, pad.offTicks, pad.count);
        }
    }

    DisneyInfinity::ResponsePacket response =
        DisneyInfinity::FlashAllPacket::buildResponse(packet.cid());
    sendPacket(response);

    _padStateDirty = true;
}

void InfinityToypad::_handleUnk95(const DisneyInfinity::CommandPacket& packet) {
    log_dbg("[InfinityToypad] UNK95 (0x95) command received. Payload: %s",
            packet.payloadToHexString());

    DisneyInfinity::ResponsePacket response =
        DisneyInfinity::Unk95Packet::buildResponse(packet.cid());
    sendPacket(response);
}

void InfinityToypad::_handleStatus(const DisneyInfinity::CommandPacket& packet) {
    log_dbg("[InfinityToypad] STATUS command received (purpose unconfirmed) - sending blank ack.");

    DisneyInfinity::ResponsePacket response = DisneyInfinity::ResponsePacket::blank(packet.cid());
    sendPacket(response);
}
