## EmuToLife

EmuToLife is a USB emulator for LEGO Dimensions, Disney Infinity, *and* [Kamen Rider Summonride](https://kamenrider.fandom.com/wiki/Kamen_Rider_Summonride).

It lets you use a Raspberry Pi Pico 2W (or W) as a replacement for an official toys-to-life pad. 
With this, you can play the game on PS3/Wii U/PS4, *or* (retail!) Xbox 360, without the original playpad.

### Setup

1. Purchase a Pi Pico 2W, and plug it in while holding the BOOTSEL button
2. Drag/drop the `.uf2` (available on Github Releases) onto the new USB device
    - If successful, the Pico will restart.
3. On the first boot, the Pico will create a new WiFi network named `captive`.
    - Connect to the `captive` WiFi network, and it will bring you to a captive portal
    
<img src="https://github.com/FromDarkHell/EmuToLife/blob/main/images/captive.png?raw=true" alt="Captive Portal" width="auto" height="512px"/>

4. From here, it will restart, and conect to your home WiFi.
5. Then, afterwards, you can connect to it via mDNS by going to `emupad.local` in a browser.

<img src="https://github.com/FromDarkHell/EmuToLife/blob/main/images/obs64_c91i8OtHqN.gif?raw=true" alt="Web UI" width="auto" height="512px"/>

### Features

- Wi-Fi captive portal for first-time setup
- Local web UI for managing the toy/character portal (spawning tags, etc.)
- Tag reading/writing/etc handled by my [ToysToLifeLib](https://github.com/FromDarkHell/ToysToLifeLib) library
- Full **retail** Xbox 360 support, powered by [libxsm3](https://github.com/InvoxiPlayGames/libxsm3/) by [InvoxiPlayGames](https://github.com/InvoxiPlayGames)

### License

This project is licensed using [MIT](/LICENSE.md)
- This project directly includes [libxsm3](https://github.com/InvoxiPlayGames/libxsm3/), which is licensed under GNU Lesser General Public License version 2.1
    * Additionally, libxsm3 uses [ExCrypt](https://github.com/emoose/ExCrypt), licensed under the 3-clause BSD license
- A large chunk of the LEGO Dimensions implementation is based on [Berny23/LD-ToyPad-Emulator](https://github.com/Berny23/LD-ToyPad-Emulator/), alongside [node-ld](https://github.com/Berny23/node-ld)

### Support

If you for some reason want to support me financially for this project (or others I make), you can donate to me via [ko-fi](https://ko-fi.com/fromdarkhell) or [Patreon](https://patreon.com/fromdarkhell).

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/O4O44GLCD) [![Support me on Patreon](https://img.shields.io/endpoint.svg?url=https%3A%2F%2Fshieldsio-patreon.vercel.app%2Fapi%3Fusername%3Dfromdarkhell%26type%3Dpatrons&style=for-the-badge)](https://patreon.com/fromdarkhell)