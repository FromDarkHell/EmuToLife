const PLATFORM_PS3 = 0;
const PLATFORM_X360 = 1;

// Games supported by the Toy Creator / toybox. `cdnPath` and `jsonPath` are
// the per-game subdirectories under the images CDN and the firmware's
// /json/ endpoint respectively. `padGridId` is the DOM id of the game's pad
// grid in index.html, and `platforms` lists the platforms the game's portal
// has a USB identity for (mirrors the firmware's toyGameSupportsPlatform()).
const GAMES = {
  dimensions: {
    label: "LEGO Dimensions",
    cdnPath: "dimensions",
    jsonPath: "dimensions",
    padGridId: "dimensions-pads",
    platforms: [PLATFORM_PS3, PLATFORM_X360],
  },
  infinity: {
    label: "Disney Infinity",
    cdnPath: "infinity",
    jsonPath: "infinity",
    padGridId: "infinity-pads",
    platforms: [PLATFORM_PS3, PLATFORM_X360],
  },
  kamenrider: {
    label: "Kamen Rider SummonRide",
    cdnPath: "kamenrider",
    jsonPath: "kamenrider",
    padGridId: "kamenrider-pads",
    platforms: [PLATFORM_PS3],
  },
};

const PLATFORM_LABELS = {
  [PLATFORM_PS3]: "PS3",
  [PLATFORM_X360]: "Xbox 360",
};

// Overwritten with the firmware's persisted choice once /settings is fetched, defaults to Dimensions + PS3 because
var selectedGame = "dimensions";
var selectedPlatform = PLATFORM_PS3;

function gameSupportsPlatform(game, platform) {
  return GAMES[game].platforms.includes(platform);
}

function cdnForGame(game) {
  return `https://raw.githubusercontent.com/FromDarkHell/EmuToLife/refs/heads/main/firmware/images/${GAMES[game].cdnPath}`;
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

function slotsForLocation(location) {
  return Array.from(
    document.querySelectorAll(
      `#${activePadGridId()} [data-pad-index^="${location}-"], #${activePadGridId()} [data-pad-index="${location}"]`,
    ),
  ).map((el) => el.getAttribute("data-pad-index"));
}

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
            index: parseInt(property, 10),
          }),
        }),
      );
    }

    target[property] = value;
    return true;
  },
};

var toypadToys = new Proxy(_toypadToys, toypadUpdateHandler);

// Populated dynamically from the active game's /json/<game>/tags.json - keyed by whatever
// category keys that file has (e.g. "characters"/"vehicles" for Dimensions, "characters"/
// "playsets"/"abilities"/"powerdiscs" for Infinity). See grid.js's loadGameData().
var _allTags = {};

// Friendly display names for known categories, used for button/modal text ("Create X" / "Select
// a X"). Falls back to a capitalized, de-pluralized guess for anything not listed here, so a new
// category added to a game's tags.json works without a frontend change.
const CATEGORY_LABELS = {
  characters: "Character",
  vehicles: "Vehicle",
  playsets: "Playset",
  abilities: "Ability",
  powerdiscs: "Power Disc",
};

function categoryLabel(category) {
  if (CATEGORY_LABELS[category]) return CATEGORY_LABELS[category];

  const singular = category.endsWith("ies")
    ? category.slice(0, -3) + "y"
    : category.endsWith("s")
      ? category.slice(0, -1)
      : category;
  return singular.charAt(0).toUpperCase() + singular.slice(1);
}

// Dimensions' backend (ToysToLifeLib) distinguishes vehicle/character tags by a singular "type"
// string; Infinity has no such distinction and just stores whatever category key it's given
// verbatim. This only needs to translate Dimensions' two known categories.
const DIMENSIONS_TYPE_OVERRIDE = {
  characters: "character",
  vehicles: "vehicle",
};

function toyTypeForCategory(category) {
  if (selectedGame === "dimensions") {
    return DIMENSIONS_TYPE_OVERRIDE[category] || category;
  }
  return category;
}

// A toy's stored "type" is Dimensions' singular form ("character"/"vehicle") or Infinity's
// already-plural category key ("playsets", etc) - both are pluralized CDN folder names, so this
// just needs to make sure singular ones get their "s" added.
function imageFolderForType(type) {
  return type.endsWith("s") ? type : `${type}s`;
}

// The DOM id of the pad grid for the currently active game (see index.html: #dimensions-pads has
// LEGO's 7 sub-positions, #infinity-pads has Infinity's 3 stacked pad sections, #kamenrider-pads
// has the Ride Gate's slots). Every grid's pad elements reuse the same small data-pad-index
// numbers, so anything that queries pad elements by index must scope to whichever grid is
// actually active - never query `.pad[data-pad-index]` document-wide.
function activePadGridId() {
  return GAMES[selectedGame].padGridId;
}

// Fetches the pad info (platform + game) from the firmware
async function loadSettings() {
  try {
    const res = await fetch("/settings");
    const settings = await res.json();

    if (GAMES[settings.game]) {
      selectedGame = settings.game;
      cdn = cdnForGame(selectedGame);
    }
    selectedPlatform = settings.platform;

    document.querySelectorAll("#game-select button").forEach((b) => {
      b.classList.toggle("active", b.dataset.game === selectedGame);
      b.classList.toggle(
        "unsupported",
        !gameSupportsPlatform(b.dataset.game, selectedPlatform),
      );
    });

    document.querySelectorAll("#platform-select button").forEach((b) => {
      b.classList.toggle(
        "active",
        Number(b.dataset.platform) === selectedPlatform,
      );
      b.classList.toggle(
        "unsupported",
        !gameSupportsPlatform(selectedGame, Number(b.dataset.platform)),
      );
    });

    // Show only the pad grid for the active game - see activePadGridId()'s comment for why the
    // others must stay out of the click-handling/rendering path entirely, not just hidden.
    const activeGridId = activePadGridId();
    Object.values(GAMES).forEach((game) => {
      document.getElementById(game.padGridId).style.display =
        game.padGridId === activeGridId ? "grid" : "none";
    });
  } catch (e) {
    console.error("[settings] Failed to load current platform/game:", e);
  }
}
