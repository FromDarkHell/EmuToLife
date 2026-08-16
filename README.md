## EmuToLife

EmuToLife is a USB emulator for the LEGO Dimensions toy pad (and *soon* **OTHER** devices). It lets you use a Raspberry Pi Pico 2W (or W) as a replacement for an official device. With this, you can play the game on PS3/Wii U/PS4, *or* Xbox 360 without the original playpad.

### Setup

1. Purchase a Pi Pico 2W, and plug it in while holding the BOOTSEL button
2. Drag/drop the `.uf2` (available on Github Releases) onto the new USB device
    - If successful, the Pico will restart.
3. On the first boot, the Pico will create a new WiFi network named `captive`.
    - Connect to the `captive` WiFi network, and it will bring you to a captive portal
    
    ![Captive Portal](/images/captive.png)

4. From here, it will restart, and conect to your home WiFi.
5. Then, afterwards, you can connect to it via mDNS by going to `emupad.local` in a browser.

![Web UI](/images/obs64_c91i8OtHqN.gif)

### Features

- Wi-Fi captive portal for first-time setup
- Local web UI for managing the toy/character portal (spawning minifigures, vehicles, etc.)
- OTA firmware updates (ElegantOTA)
- Tag reading/writing/etc handled by my [ToysToLifeLib](https://github.com/FromDarkHell/ToysToLifeLib) library

### Support

If you for some reason want to support me financially for this project (or others I make), you can donate to me via [ko-fi](https://ko-fi.com/fromdarkhell) or [Patreon](https://patreon.com/fromdarkhell).

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/O4O44GLCD) [![Support me on Patreon](https://img.shields.io/endpoint.svg?url=https%3A%2F%2Fshieldsio-patreon.vercel.app%2Fapi%3Fusername%3Dfromdarkhell%26type%3Dpatrons&style=for-the-badge)](https://patreon.com/fromdarkhell)