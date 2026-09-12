#include <Arduino.h>
#include <led/led_controller.h>
#include "config/config.h"
#include "infinity/toypad.h"
#include "lego/toypad.h"
#include "log/logger.h"
#include "RP2040Support.h"
#include "web/captive_portal.h"
#include "web/server.h"

CaptivePortal* initializationPortal = nullptr;
LegoServer* webServer = nullptr;

// Exactly one PortalTransport-derived object is ever constructed, chosen by the persisted game -
// LEGO Dimensions and Disney Infinity are mutually exclusive USB device identities per boot, so
// there's no need (and no room, USB-wise) for both to exist at once.
PortalTransport* activePad = nullptr;
ToyGame activeGame = ToyGame::LegoDimensions;

PlaypadLEDController<4>* padLEDs = nullptr;

void create_captive_portal() {
    logger.blinkStatus(5, 1000);
    activePad->getPad(PadLocation::CENTER)->setColor(PadColor::Red());

    log_dbg("[create_captive_portal] Creating captive portal...");
    initializationPortal = new CaptivePortal("captive", NULL);
    log_dbg("[create_captive_portal] Waiting for initialization...");
}

void setup() {
#ifdef PICO_RP2040
    // Enable the double reset detector,
    // So that if the user double-taps the reset button, it'll boot into the UF2 bootloader for easy
    // flashing
    rp2040.enableDoubleResetBootloader();
#endif

    randomSeed(analogRead(A1));

    log_init();
    config.init();

    activeGame = static_cast<ToyGame>(
        config.getUChar("game", static_cast<uint8_t>(ToyGame::LegoDimensions)));

    if (activeGame == ToyGame::DisneyInfinity) {
        InfinityToypad* pad = new InfinityToypad();
        pad->begin();
        activePad = pad;
    } else {
        Toypad* pad = new Toypad();
        pad->begin();
        activePad = pad;
    }

    padLEDs = new PlaypadLEDController<4>(activePad, {3, 1, 3});
    padLEDs->begin();

    if (config.getBool("initialized", false)) {
        log_dbg("[setup] Device already initialized, skipping captive portal...");
        logger.blinkStatus(2, 100);

        if (activePad->platform() == ToypadPlatform::PS3) {
            activePad->getPad(PadLocation::CENTER)->setFade(PadColor::Blue(), 50, 2);
        } else {
            activePad->getPad(PadLocation::CENTER)->setFade(PadColor::Green(), 50, 2);
        }

        if (activeGame == ToyGame::LegoDimensions) {
            activePad->getPad(PadLocation::LEFT)->setFade(PadColor::Blue(), 50, 2);
            activePad->getPad(PadLocation::RIGHT)->setFade(PadColor::Blue(), 50, 2);
        } else if (activeGame == ToyGame::DisneyInfinity) {
            activePad->getPad(PadLocation::LEFT)->setFade(PadColor::White(), 50, 2);
            activePad->getPad(PadLocation::RIGHT)->setFade(PadColor::White(), 50, 2);
        }

        return;
    }

    log_dbg("[setup] Device not initialized, creating captive portal...");
    create_captive_portal();
}

void loop() {
    // Updates the current USB state (i.e. sending events/etc/etc), plus fade/flash pad state.
    activePad->update();
    padLEDs->update();

    if (initializationPortal != nullptr) {
        // For the sake of seeing if the portal is initialized, just blink the LED on/off while we
        // check for DNS requests/etc.
        initializationPortal->loop();

        // Check if the credentials have been saved, if so, it's time to initialize our connection
        // to the WiFi instance.
        if (initializationPortal->credentialsSaved) {
            delete initializationPortal;
            initializationPortal = nullptr;
        }

        return;
    }

    if (webServer == nullptr) {
        log_dbg("[loop] Initializing Playpad Server...");

        const char* ssid = config.getString("ssid", "");
        const char* pass = config.getString("password", "");

        // server.cpp is game-agnostic - it only ever calls generic PortalTransport/Toy interface
        // methods (placeToy(), game(), onTagStateChange(), etc), so the single active portal
        // object works here regardless of which game it's emulating.
        webServer = new LegoServer(ssid, pass, activePad);

        free((void*)ssid);
        free((void*)pass);

        if (!webServer->success) {
            delete webServer;
            webServer = nullptr;

            create_captive_portal();
            return;
        }

        assert(webServer->success);
    }

    if (webServer != nullptr) {
        webServer->loop();
    }

#ifdef TINYUSB_NEED_POLLING_TASK
    // Manual call tud_task since it isn't called by Core's background
    TinyUSBDevice.task();
#endif
}
