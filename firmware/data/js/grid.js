// Each entry is one optional filter dropdown a category's tags can offer, built only if at least
// one tag actually has a non-empty value for `key`. `array: true` means `key` holds a list of
// values per tag (e.g. LEGO's "abilities"); otherwise it's a single scalar (e.g. Infinity's
// "gameVersion").
const FILTER_DEFINITIONS = [
  {
    key: "abilities",
    label: "Ability",
    allLabel: "All Abilities",
    array: true,
  },
  {
    key: "world",
    label: "Franchise",
    allLabel: "All Franchises",
    array: false,
  },

  {
    key: "gameVersion",
    label: "Game Version",
    allLabel: "All Versions",
    array: false,
  },
  {
    key: "playset",
    label: "Playset",
    allLabel: "All Playsets",
    array: true,
  },
];

// All of a tag's (and its rebuilds', if any) values for one filter field.
function filterValues(tag, def) {
  const builds = [tag].concat(tag.rebuilds ?? []);
  const values = [];

  builds.forEach((build) => {
    if (def.array) {
      (build[def.key] ?? []).forEach((v) => values.push(v));
    } else if (
      build[def.key] !== undefined &&
      build[def.key] !== null &&
      build[def.key] !== ""
    ) {
      values.push(build[def.key]);
    }
  });

  return values;
}

function sortFilterValues(values) {
  return [...values].sort((a, b) =>
    typeof a === "number" && typeof b === "number"
      ? a - b
      : String(a).localeCompare(String(b)),
  );
}

function populateFilterDropdowns(category, tags) {
  FILTER_DEFINITIONS.forEach((def) => {
    const select = document.getElementById(`${category}Filter_${def.key}`);
    if (!select) return;

    const values = new Set();
    tags.forEach((tag) => filterValues(tag, def).forEach((v) => values.add(v)));

    // Preserve current selection if it still exists after a reload
    const current = select.value;
    select.innerHTML = `<option value="">${def.allLabel}</option>`;

    sortFilterValues([...values].filter((v) => v !== "")).forEach((value) => {
      const opt = document.createElement("option");
      opt.value = value;
      opt.textContent = value;
      select.appendChild(opt);
    });

    if (current && [...values].some((v) => String(v) === current)) {
      select.value = current;
    }
  });
}

function getFilteredTags(category) {
  let tags = _allTags[category] ?? [];

  FILTER_DEFINITIONS.forEach((def) => {
    const select = document.getElementById(`${category}Filter_${def.key}`);
    const chosen = select ? select.value : "";
    if (!chosen) return;

    tags = tags.filter((tag) =>
      filterValues(tag, def).some((v) => String(v) === chosen),
    );
  });

  return tags;
}

// Renders one tag's grid entry. Tags with a `rebuilds` array (LEGO Dimensions vehicles) get the
// expandable "pick a specific build" treatment; everything else is a single clickable image.
function updateGrid(category, tags) {
  const grid = document.getElementById(`${category}Grid`);
  if (!grid) return;

  grid.innerHTML = "";

  const folder = imageFolderForType(category);
  const sorted = [...tags].sort((a, b) => a.name.localeCompare(b.name));

  sorted.forEach((tag) => {
    const option = document.createElement("div");
    option.className = "character-option";

    if (tag.rebuilds && tag.rebuilds.length > 0) {
      const contents = [];
      for (const build of [tag].concat(tag.rebuilds)) {
        const el = document.createElement("div");
        el.className = "character-option";
        el.addEventListener("click", (evt) =>
          createNewToy(evt.target, build, category),
        );
        el.innerHTML = `
                <img src="${cdn}/${folder}/${build.id.toString().padStart(4, "0")}.webp"
                     alt="${build.name}" ${cdnFallbackScript}>
                <p>${build.name}</p>
            `;
        contents.push(el);
      }

      option.innerHTML = `
            <details>
              <summary style="display:flex;flex-direction:row;gap:1em;
                              justify-content:center;align-items:center">
                <img src="${cdn}/${folder}/${tag.id.toString().padStart(4, "0")}.webp"
                     alt="${tag.name}" height="64" width="64" ${cdnFallbackScript}>
                <div>${tag.name}</div>
              </summary>
              <div class="tagList" style="display:flex;flex-direction:row;gap:1em;
                          justify-content:center;align-items:center;margin:20px 0">
              </div>
            </details>
        `;

      option.querySelector(".tagList").append(...contents);
    } else {
      option.innerHTML = `
            <img src="${cdn}/${folder}/${tag.id.toString().padStart(4, "0")}.webp"
                 alt="${tag.name}" ${cdnFallbackScript}>
            <p>${tag.name}</p>
        `;
      option.addEventListener("click", (evt) =>
        createNewToy(evt.target, tag, category),
      );
    }

    grid.appendChild(option);
  });
}

function refreshGrid(category) {
  updateGrid(category, getFilteredTags(category));
}

// Sends a new request to the server, creating a new toy for inside the toybox
async function createNewToy(element, tag, category) {
  await fetch("/toybox", {
    method: "PUT",
    headers: {
      "Content-Type": "application/x-www-form-urlencoded",
    },
    body: new URLSearchParams({
      name: tag.name,
      id: tag.id,
      type: toyTypeForCategory(category),
      game: selectedGame,
    }),
  }).then((x) => {
    updateToybox().then((x) => {
      // Close the modal
      element.closest(".modal")?.classList.remove("active");
    });
  });
}

// Builds the "Create X" button + selection modal for one category. A no-op if they already exist
// (e.g. a stale re-run) - callers should only invoke this once per category per page load.
function buildCategoryUI(category) {
  const label = categoryLabel(category);
  const modalId = `${category}Modal`;

  if (document.getElementById(modalId)) return;

  const tags = _allTags[category] ?? [];
  const hasRebuilds = tags.some(
    (tag) => tag.rebuilds && tag.rebuilds.length > 0,
  );

  const relevantFilters = FILTER_DEFINITIONS.filter((def) =>
    tags.some((tag) => filterValues(tag, def).length > 0),
  );

  const btn = document.createElement("button");
  btn.setAttribute("data-modal-open", `#${modalId}`);
  btn.textContent = `Create ${label}`;
  document.getElementById("toyCreatorButtons").appendChild(btn);

  const filterSelectsHtml = relevantFilters
    .map(
      (def) => `
          <select id="${category}Filter_${def.key}" class="filter-select">
            <option value="">${def.allLabel}</option>
          </select>`,
    )
    .join("");

  const modal = document.createElement("div");
  modal.id = modalId;
  modal.className = "modal";
  modal.innerHTML = `
    <div class="modal-content">
      <span class="modal-close" data-modal-close>&times;</span>
      <div>
        <div class="filter-bar">
          <h2 style="margin: 0">Select a ${label}</h2>
          ${filterSelectsHtml}
        </div>
        <div id="${category}Grid" class="tag-grid${hasRebuilds ? " has-rebuilds" : ""}">
          <!-- Populated dynamically -->
        </div>
      </div>
    </div>
  `;
  document.getElementById("toyCreatorModals").appendChild(modal);

  relevantFilters.forEach((def) => {
    document
      .getElementById(`${category}Filter_${def.key}`)
      .addEventListener("change", () => refreshGrid(category));
  });
}

// Fetches the current game's combined tag data (one tags.json with a key per category, e.g.
// "characters"/"vehicles" for Dimensions or "characters"/"playsets"/"abilities"/"powerdiscs" for
// Infinity) and builds a "Create X" button/grid for each category found.
async function loadGameData() {
  const jsonPath = GAMES[selectedGame].jsonPath;

  document.getElementById("toyCreatorButtons").innerHTML = "";
  document.getElementById("toyCreatorModals").innerHTML = "";
  _allTags = {};

  const res = await fetch(`/json/${jsonPath}/tags.json`);
  const data = res.ok ? await res.json() : {};

  for (const category of Object.keys(data)) {
    _allTags[category] = data[category];
    buildCategoryUI(category);
    populateFilterDropdowns(category, _allTags[category]);
    refreshGrid(category);
  }
}

// Loads the current game's tag data / Toy Creator UI. Expects loadSettings() to have already
// resolved (see app.js's init()), so `cdn`/`selectedGame` are correct before anything renders.
function initGrid() {
  loadGameData();
}
