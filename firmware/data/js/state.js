// Games supported by the Toy Creator / toybox. `cdnPath` and `jsonPath` are
// the per-game subdirectories under the images CDN and the firmware's
// /json/ endpoint respectively.
const GAMES = {
  dimensions: {
    label: "LEGO Dimensions",
    cdnPath: "dimensions",
    jsonPath: "dimensions",
  },
  infinity: {
    label: "Disney Infinity",
    cdnPath: "infinity",
    jsonPath: "infinity",
  },
};

// Overwritten with the firmware's persisted choice once /settings is fetched (see grid.js's
// loadSettings()); "dimensions" is just what's rendered for the instant before that resolves.
var selectedGame = "dimensions";

function cdnForGame(game) {
  return `http://192.168.1.236:8000/${GAMES[game].cdnPath}`;
  // return `https://raw.githubusercontent.com/FromDarkHell/EmuToLife/refs/heads/main/firmware/images/${GAMES[game].cdnPath}`;
}

var cdn = cdnForGame(selectedGame);

// Simple CDN fallback which sets onError
const cdnFallbackScript = `onerror="this.onerror=null;this.src='/images/blank.webp'"`;

var characters = null;
var vehicles = null;

var selectedToy = null;
var toybox = [];

// Object to track toys on toypad: {padId: toyObject}
var _toypadToys = {};

const _requestQueue = {
  _chain: Promise.resolve(),
  add(fn) {
    this._chain = this._chain.then(fn).catch((err) => {
      console.error("[Queue] Request failed:", err);
    });
    return this._chain;
  },
};

const toypadUpdateHandler = {
  set(target, property, value) {
    const oldCharacter = target[property];
    const newCharacter = value;

    // Only send a removal if the toy is going back to the toybox (index -1)
    // For pad-to-pad moves, the backend handles the remove itself
    if (oldCharacter !== undefined && newCharacter === undefined) {
      _requestQueue.add(() =>
        fetch("/toypad", {
          method: "PUT",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: new URLSearchParams({ uid: oldCharacter.uid, index: -1 }),
        }),
      );
    }

    if (newCharacter !== undefined) {
      _requestQueue.add(() =>
        fetch("/toypad", {
          method: "PUT",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: new URLSearchParams({
            uid: newCharacter.uid,
            index: property,
          }),
        }),
      );
    }

    target[property] = value;
    return true;
  },
};

var toypadToys = new Proxy(_toypadToys, toypadUpdateHandler);

const _allTags = { character: [], vehicle: [] };

// Fetches the pad info (platform + game) from the firmware
async function loadSettings() {
  try {
    const res = await fetch("/settings");
    const settings = await res.json();

    if (GAMES[settings.game]) {
      selectedGame = settings.game;
      cdn = cdnForGame(selectedGame);
    }

    document
      .querySelectorAll("#game-select button")
      .forEach((b) =>
        b.classList.toggle("active", b.dataset.game === selectedGame),
      );

    document
      .querySelectorAll("#platform-select button")
      .forEach((b) =>
        b.classList.toggle(
          "active",
          Number(b.dataset.platform) === settings.platform,
        ),
      );
  } catch (e) {
    console.error("[settings] Failed to load current platform/game:", e);
  }
}
