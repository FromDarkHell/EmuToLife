const cdn =
  "https://raw.githubusercontent.com/FromDarkHell/EmuToLife/refs/heads/main/firmware/images/dimensions";

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
